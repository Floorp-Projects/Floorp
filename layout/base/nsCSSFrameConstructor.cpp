/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * construction of a frame tree that is nearly isomorphic to the content
 * tree and updating of that tree in response to dynamic changes
 */

#include "nsCSSFrameConstructor.h"

#include "mozilla/AutoRestore.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/dom/HTMLDetailsElement.h"
#include "mozilla/dom/HTMLSelectElement.h"
#include "mozilla/dom/HTMLSummaryElement.h"
#include "mozilla/EventStates.h"
#include "mozilla/Likely.h"
#include "mozilla/LinkedList.h"
#include "mozilla/PresShell.h"
#include "nsAbsoluteContainingBlock.h"
#include "nsCSSPseudoElements.h"
#include "nsIAtom.h"
#include "nsIFrameInlines.h"
#include "nsGkAtoms.h"
#include "nsPresContext.h"
#include "nsIDocument.h"
#include "nsTableFrame.h"
#include "nsTableColFrame.h"
#include "nsTableRowFrame.h"
#include "nsTableCellFrame.h"
#include "nsIDOMHTMLDocument.h"
#include "nsHTMLParts.h"
#include "nsIPresShell.h"
#include "nsUnicharUtils.h"
#include "mozilla/StyleSetHandle.h"
#include "mozilla/StyleSetHandleInlines.h"
#include "nsViewManager.h"
#include "nsStyleConsts.h"
#include "nsIDOMXULElement.h"
#include "nsContainerFrame.h"
#include "nsNameSpaceManager.h"
#include "nsIComboboxControlFrame.h"
#include "nsIListControlFrame.h"
#include "nsIDOMCharacterData.h"
#include "nsPlaceholderFrame.h"
#include "nsTableRowGroupFrame.h"
#include "nsIFormControl.h"
#include "nsCSSAnonBoxes.h"
#include "nsTextFragment.h"
#include "nsIAnonymousContentCreator.h"
#include "nsBindingManager.h"
#include "nsXBLBinding.h"
#include "nsContentUtils.h"
#include "nsIScriptError.h"
#ifdef XP_MACOSX
#include "nsIDocShell.h"
#endif
#include "ChildIterator.h"
#include "nsError.h"
#include "nsLayoutUtils.h"
#include "nsAutoPtr.h"
#include "nsBoxFrame.h"
#include "nsBoxLayout.h"
#include "nsFlexContainerFrame.h"
#include "nsGridContainerFrame.h"
#include "RubyUtils.h"
#include "nsRubyFrame.h"
#include "nsRubyBaseFrame.h"
#include "nsRubyBaseContainerFrame.h"
#include "nsRubyTextFrame.h"
#include "nsRubyTextContainerFrame.h"
#include "nsImageFrame.h"
#include "nsIObjectLoadingContent.h"
#include "nsTArray.h"
#include "nsGenericDOMDataNode.h"
#include "mozilla/dom/Element.h"
#include "nsAutoLayoutPhase.h"
#include "nsStyleStructInlines.h"
#include "nsPageContentFrame.h"
#include "mozilla/RestyleManagerHandle.h"
#include "mozilla/RestyleManagerHandleInlines.h"
#include "StickyScrollContainer.h"
#include "nsFieldSetFrame.h"
#include "nsInlineFrame.h"
#include "nsBlockFrame.h"
#include "nsCanvasFrame.h"
#include "nsFirstLetterFrame.h"
#include "nsGfxScrollFrame.h"
#include "nsPageFrame.h"
#include "nsSimplePageSequenceFrame.h"
#include "nsTableWrapperFrame.h"
#include "nsIScrollableFrame.h"
#include "nsBackdropFrame.h"
#include "nsTransitionManager.h"
#include "DetailsFrame.h"

#ifdef MOZ_XUL
#include "nsIRootBox.h"
#endif
#ifdef ACCESSIBILITY
#include "nsAccessibilityService.h"
#endif

#include "nsXBLService.h"

#undef NOISY_FIRST_LETTER

#include "nsMathMLParts.h"
#include "mozilla/dom/SVGTests.h"
#include "nsSVGUtils.h"

#include "nsRefreshDriver.h"
#include "nsRuleProcessorData.h"
#include "nsTextNode.h"
#include "ActiveLayerTracker.h"

using namespace mozilla;
using namespace mozilla::dom;

// An alias for convenience.
static const nsIFrame::ChildListID kPrincipalList = nsIFrame::kPrincipalList;

nsIFrame*
NS_NewHTMLCanvasFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

nsIFrame*
NS_NewHTMLVideoFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

nsContainerFrame*
NS_NewSVGOuterSVGFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsContainerFrame*
NS_NewSVGOuterSVGAnonChildFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGInnerSVGFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGPathGeometryFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGGFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGGenericContainerFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsContainerFrame*
NS_NewSVGForeignObjectFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGAFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGSwitchFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGTextFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGContainerFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGUseFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGViewFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
extern nsIFrame*
NS_NewSVGLinearGradientFrame(nsIPresShell *aPresShell, nsStyleContext* aContext);
extern nsIFrame*
NS_NewSVGRadialGradientFrame(nsIPresShell *aPresShell, nsStyleContext* aContext);
extern nsIFrame*
NS_NewSVGStopFrame(nsIPresShell *aPresShell, nsStyleContext* aContext);
nsContainerFrame*
NS_NewSVGMarkerFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsContainerFrame*
NS_NewSVGMarkerAnonChildFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
extern nsIFrame*
NS_NewSVGImageFrame(nsIPresShell *aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGClipPathFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGFilterFrame(nsIPresShell *aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGPatternFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGMaskFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGFEContainerFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGFELeafFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGFEImageFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewSVGFEUnstyledLeafFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

#include "mozilla/dom/NodeInfo.h"
#include "prenv.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"

#ifdef DEBUG
// Set the environment variable GECKO_FRAMECTOR_DEBUG_FLAGS to one or
// more of the following flags (comma separated) for handy debug
// output.
static bool gNoisyContentUpdates = false;
static bool gReallyNoisyContentUpdates = false;
static bool gNoisyInlineConstruction = false;

struct FrameCtorDebugFlags {
  const char* name;
  bool* on;
};

static FrameCtorDebugFlags gFlags[] = {
  { "content-updates",              &gNoisyContentUpdates },
  { "really-noisy-content-updates", &gReallyNoisyContentUpdates },
  { "noisy-inline",                 &gNoisyInlineConstruction }
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

nsContainerFrame*
NS_NewRootBoxFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

nsContainerFrame*
NS_NewDocElementBoxFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

nsIFrame*
NS_NewDeckFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

nsIFrame*
NS_NewLeafBoxFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

nsIFrame*
NS_NewStackFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

nsIFrame*
NS_NewProgressMeterFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

nsIFrame*
NS_NewRangeFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

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
NS_NewMenuFrame (nsIPresShell* aPresShell, nsStyleContext* aContext, uint32_t aFlags);

nsIFrame*
NS_NewMenuBarFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

nsIFrame*
NS_NewTreeBodyFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

// grid
nsresult
NS_NewGridLayout2 ( nsIPresShell* aPresShell, nsBoxLayout** aNewLayout );
nsIFrame*
NS_NewGridRowLeafFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame*
NS_NewGridRowGroupFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

// end grid

nsIFrame*
NS_NewTitleBarFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

nsIFrame*
NS_NewResizerFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);


#endif

nsHTMLScrollFrame*
NS_NewHTMLScrollFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, bool aIsRoot);

nsXULScrollFrame*
NS_NewXULScrollFrame(nsIPresShell* aPresShell, nsStyleContext* aContext,
                     bool aIsRoot, bool aClipAllDescendants);

nsIFrame*
NS_NewSliderFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

nsIFrame*
NS_NewScrollbarFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);

nsIFrame*
NS_NewScrollbarButtonFrame (nsIPresShell* aPresShell, nsStyleContext* aContext);


#ifdef NOISY_FINDFRAME
static int32_t FFWC_totalCount=0;
static int32_t FFWC_doLoop=0;
static int32_t FFWC_doSibling=0;
static int32_t FFWC_recursions=0;
static int32_t FFWC_nextInFlows=0;
#endif

// Returns true if aFrame is an anonymous flex/grid item.
static inline bool
IsAnonymousFlexOrGridItem(const nsIFrame* aFrame)
{
  const nsIAtom* pseudoType = aFrame->StyleContext()->GetPseudo();
  return pseudoType == nsCSSAnonBoxes::anonymousFlexItem ||
         pseudoType == nsCSSAnonBoxes::anonymousGridItem;
}

// Returns true if aFrame is a flex/grid container.
static inline bool
IsFlexOrGridContainer(const nsIFrame* aFrame)
{
  const nsIAtom* t = aFrame->GetType();
  return t == nsGkAtoms::flexContainerFrame ||
         t == nsGkAtoms::gridContainerFrame;
}

// Returns true IFF the given nsIFrame is a nsFlexContainerFrame and
// represents a -webkit-{inline-}box container. The frame's GetType() result is
// passed as a separate argument, since in most cases, the caller will have
// looked it up already, and we'd rather not make redundant calls to the
// virtual GetType() method.
static inline bool
IsFlexContainerForLegacyBox(const nsIFrame* aFrame,
                            const nsIAtom* aFrameType)
{
  MOZ_ASSERT(aFrame->GetType() == aFrameType, "wrong aFrameType was passed");
  return aFrameType == nsGkAtoms::flexContainerFrame &&
    aFrame->HasAnyStateBits(NS_STATE_FLEX_IS_LEGACY_WEBKIT_BOX);
}

#if DEBUG
static void
AssertAnonymousFlexOrGridItemParent(const nsIFrame* aChild,
                                    const nsIFrame* aParent)
{
  MOZ_ASSERT(IsAnonymousFlexOrGridItem(aChild),
             "expected an anonymous flex or grid item child frame");
  MOZ_ASSERT(aParent, "expected a parent frame");
  const nsIAtom* pseudoType = aChild->StyleContext()->GetPseudo();
  if (pseudoType == nsCSSAnonBoxes::anonymousFlexItem) {
    MOZ_ASSERT(aParent->GetType() == nsGkAtoms::flexContainerFrame,
               "anonymous flex items should only exist as children "
               "of flex container frames");
  } else {
    MOZ_ASSERT(aParent->GetType() == nsGkAtoms::gridContainerFrame,
               "anonymous grid items should only exist as children "
               "of grid container frames");
  }
}
#else
#define AssertAnonymousFlexOrGridItemParent(x, y) do { /* nothing */ } while(0)
#endif

static inline nsContainerFrame*
GetFieldSetBlockFrame(nsIFrame* aFieldsetFrame)
{
  // Depends on the fieldset child frame order - see ConstructFieldSetFrame() below.
  nsIFrame* firstChild = aFieldsetFrame->PrincipalChildList().FirstChild();
  nsIFrame* inner = firstChild && firstChild->GetNextSibling() ? firstChild->GetNextSibling() : firstChild;
  return inner ? inner->GetContentInsertionFrame() : nullptr;
}

#define FCDATA_DECL(_flags, _func)                            \
  { _flags, { (FrameCreationFunc)_func }, nullptr, nullptr }
#define FCDATA_WITH_WRAPPING_BLOCK(_flags, _func, _anon_box)  \
  { _flags | FCDATA_CREATE_BLOCK_WRAPPER_FOR_ALL_KIDS,        \
      { (FrameCreationFunc)_func }, nullptr, &_anon_box }

#define UNREACHABLE_FCDATA()                                  \
  { 0, { (FrameCreationFunc)nullptr }, nullptr, nullptr }
//----------------------------------------------------------------------

/**
 * True if aFrame is an actual inline frame in the sense of non-replaced
 * display:inline CSS boxes.  In other words, it can be affected by {ib}
 * splitting and can contain first-letter frames.  Basically, this is either an
 * inline frame (positioned or otherwise) or an line frame (this last because
 * it can contain first-letter and because inserting blocks in the middle of it
 * needs to terminate it).
 */
static bool
IsInlineFrame(const nsIFrame* aFrame)
{
  return aFrame->IsFrameOfType(nsIFrame::eLineParticipant);
}

/**
 * True if aFrame is an instance of an SVG frame class or is an inline/block
 * frame being used for SVG text.
 */
static bool
IsFrameForSVG(const nsIFrame* aFrame)
{
  return aFrame->IsFrameOfType(nsIFrame::eSVG) ||
         aFrame->IsSVGText();
}

/**
 * Returns true iff aFrame explicitly prevents its descendants from floating
 * (at least, down to the level of descendants which themselves are
 * float-containing blocks -- those will manage the floating status of any
 * lower-level descendents inside them, of course).
 */
static bool
ShouldSuppressFloatingOfDescendants(nsIFrame* aFrame)
{
  return aFrame->IsFrameOfType(nsIFrame::eMathML) ||
    aFrame->IsXULBoxFrame() ||
    ::IsFlexOrGridContainer(aFrame);
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
  return nullptr;
}

// Reparent a frame into a wrapper frame that is a child of its old parent.
static void
ReparentFrame(RestyleManagerHandle aRestyleManager,
              nsContainerFrame* aNewParentFrame,
              nsIFrame* aFrame)
{
  aFrame->SetParent(aNewParentFrame);
  aRestyleManager->ReparentStyleContext(aFrame);
}

static void
ReparentFrames(nsCSSFrameConstructor* aFrameConstructor,
               nsContainerFrame* aNewParentFrame,
               const nsFrameList& aFrameList)
{
  RestyleManagerHandle restyleManager = aFrameConstructor->RestyleManager();
  for (nsFrameList::Enumerator e(aFrameList); !e.AtEnd(); e.Next()) {
    ReparentFrame(restyleManager, aNewParentFrame, e.get());
  }
}

//----------------------------------------------------------------------
//
// When inline frames get weird and have block frames in them, we
// annotate them to help us respond to incremental content changes
// more easily.

static inline bool
IsFramePartOfIBSplit(nsIFrame* aFrame)
{
  return (aFrame->GetStateBits() & NS_FRAME_PART_OF_IBSPLIT) != 0;
}

static nsContainerFrame* GetIBSplitSibling(nsIFrame* aFrame)
{
  NS_PRECONDITION(IsFramePartOfIBSplit(aFrame), "Shouldn't call this");

  // We only store the "ib-split sibling" annotation with the first
  // frame in the continuation chain. Walk back to find that frame now.
  return static_cast<nsContainerFrame*>
    (aFrame->FirstContinuation()->
       Properties().Get(nsIFrame::IBSplitSibling()));
}

static nsContainerFrame* GetIBSplitPrevSibling(nsIFrame* aFrame)
{
  NS_PRECONDITION(IsFramePartOfIBSplit(aFrame), "Shouldn't call this");

  // We only store the ib-split sibling annotation with the first
  // frame in the continuation chain. Walk back to find that frame now.
  return static_cast<nsContainerFrame*>
    (aFrame->FirstContinuation()->
       Properties().Get(nsIFrame::IBSplitPrevSibling()));
}

static nsContainerFrame*
GetLastIBSplitSibling(nsIFrame* aFrame, bool aReturnEmptyTrailingInline)
{
  for (nsIFrame *frame = aFrame, *next; ; frame = next) {
    next = GetIBSplitSibling(frame);
    if (!next ||
        (!aReturnEmptyTrailingInline && !next->PrincipalChildList().FirstChild() &&
         !GetIBSplitSibling(next))) {
      NS_ASSERTION(!next || !frame->IsInlineOutside(),
                   "Should have a block here!");
      return static_cast<nsContainerFrame*>(frame);
    }
  }
  NS_NOTREACHED("unreachable code");
  return nullptr;
}

static void
SetFrameIsIBSplit(nsContainerFrame* aFrame, nsIFrame* aIBSplitSibling)
{
  NS_PRECONDITION(aFrame, "bad args!");

  // We should be the only continuation
  NS_ASSERTION(!aFrame->GetPrevContinuation(),
               "assigning ib-split sibling to other than first continuation!");
  NS_ASSERTION(!aFrame->GetNextContinuation() ||
               IsFramePartOfIBSplit(aFrame->GetNextContinuation()),
               "should have no non-ib-split continuations here");

  // Mark the frame as ib-split.
  aFrame->AddStateBits(NS_FRAME_PART_OF_IBSPLIT);

  if (aIBSplitSibling) {
    NS_ASSERTION(!aIBSplitSibling->GetPrevContinuation(),
                 "assigning something other than the first continuation as the "
                 "ib-split sibling");

    // Store the ib-split sibling (if we were given one) with the
    // first frame in the flow.
    FramePropertyTable* props = aFrame->PresContext()->PropertyTable();
    props->Set(aFrame, nsIFrame::IBSplitSibling(), aIBSplitSibling);
    props->Set(aIBSplitSibling, nsIFrame::IBSplitPrevSibling(), aFrame);
  }
}

static nsIFrame*
GetIBContainingBlockFor(nsIFrame* aFrame)
{
  NS_PRECONDITION(IsFramePartOfIBSplit(aFrame),
                  "GetIBContainingBlockFor() should only be called on known IB frames");

  // Get the first "normal" ancestor of the target frame.
  nsIFrame* parentFrame;
  do {
    parentFrame = aFrame->GetParent();

    if (! parentFrame) {
      NS_ERROR("no unsplit block frame in IB hierarchy");
      return aFrame;
    }

    // Note that we ignore non-ib-split frames which have a pseudo on their
    // style context -- they're not the frames we're looking for!  In
    // particular, they may be hiding a real parent that _is_ in an ib-split.
    if (!IsFramePartOfIBSplit(parentFrame) &&
        !parentFrame->StyleContext()->GetPseudo())
      break;

    aFrame = parentFrame;
  } while (1);

  // post-conditions
  NS_ASSERTION(parentFrame, "no normal ancestor found for ib-split frame "
                            "in GetIBContainingBlockFor");
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

// After this function returns, aLink is pointing to the first link at or
// after its starting position for which the next frame is a block.  If there
// is no such link, it points to the end of the list.
static void
FindFirstBlock(nsFrameList::FrameLinkEnumerator& aLink)
{
  for ( ; !aLink.AtEnd(); aLink.Next()) {
    if (!aLink.NextFrame()->IsInlineOutside()) {
      return;
    }
  }
}

// This function returns a frame link enumerator pointing to the first link in
// the list for which the next frame is not block.  If there is no such link,
// it points to the end of the list.
static nsFrameList::FrameLinkEnumerator
FindFirstNonBlock(const nsFrameList& aList)
{
  nsFrameList::FrameLinkEnumerator link(aList);
  for (; !link.AtEnd(); link.Next()) {
    if (link.NextFrame()->IsInlineOutside()) {
      break;
    }
  }
  return link;
}

inline void
SetInitialSingleChild(nsContainerFrame* aParent, nsIFrame* aFrame)
{
  NS_PRECONDITION(!aFrame->GetNextSibling(), "Should be using a frame list");
  nsFrameList temp(aFrame, aFrame);
  aParent->SetInitialChildList(kPrincipalList, temp);
}

// -----------------------------------------------------------

// Structure used when constructing formatting object trees.
struct nsFrameItems : public nsFrameList
{
  // Appends the frame to the end of the list
  void AddChild(nsIFrame* aChild);
};

void
nsFrameItems::AddChild(nsIFrame* aChild)
{
  NS_PRECONDITION(aChild, "nsFrameItems::AddChild");

  // It'd be really nice if we could just AppendFrames(kPrincipalList, aChild) here,
  // but some of our callers put frames that have different
  // parents (caption, I'm looking at you) on the same framelist, and
  // nsFrameList asserts if you try to do that.
  if (IsEmpty()) {
    SetFrames(aChild);
  }
  else {
    NS_ASSERTION(aChild != mLastChild,
                 "Same frame being added to frame list twice?");
    mLastChild->SetNextSibling(aChild);
    mLastChild = nsLayoutUtils::GetLastSibling(aChild);
  }
}

// -----------------------------------------------------------

// Structure used when constructing formatting object trees. Contains
// state information needed for absolutely positioned elements
struct nsAbsoluteItems : nsFrameItems {
  // containing block for absolutely positioned elements
  nsContainerFrame* containingBlock;

  explicit nsAbsoluteItems(nsContainerFrame* aContainingBlock);
#ifdef DEBUG
  // XXXbz Does this need a debug-only assignment operator that nulls out the
  // childList in the nsAbsoluteItems we're copying?  Introducing a difference
  // between debug and non-debug behavior seems bad, so I guess not...
  ~nsAbsoluteItems() {
    NS_ASSERTION(!FirstChild(),
                 "Dangling child list.  Someone forgot to insert it?");
  }
#endif

  // Appends the frame to the end of the list
  void AddChild(nsIFrame* aChild);
};

nsAbsoluteItems::nsAbsoluteItems(nsContainerFrame* aContainingBlock)
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

// -----------------------------------------------------------

// Structure for saving the existing state when pushing/poping containing
// blocks. The destructor restores the state to its previous state
class MOZ_STACK_CLASS nsFrameConstructorSaveState {
public:
  typedef nsIFrame::ChildListID ChildListID;
  nsFrameConstructorSaveState();
  ~nsFrameConstructorSaveState();

private:
  nsAbsoluteItems* mItems;      // pointer to struct whose data we save/restore
  nsAbsoluteItems  mSavedItems; // copy of original data

  // The name of the child list in which our frames would belong
  ChildListID mChildListID;
  nsFrameConstructorState* mState;

  // State used only when we're saving the abs-pos state for a transformed
  // element.
  nsAbsoluteItems mSavedFixedItems;

  bool mSavedFixedPosIsAbsPos;

  friend class nsFrameConstructorState;
};

// Structure used to keep track of a list of bindings we need to call
// AddToAttachedQueue on.  These should be in post-order depth-first
// flattened tree traversal order.
struct PendingBinding : public LinkedListElement<PendingBinding>
{
#ifdef NS_BUILD_REFCNT_LOGGING
  PendingBinding() {
    MOZ_COUNT_CTOR(PendingBinding);
  }
  ~PendingBinding() {
    MOZ_COUNT_DTOR(PendingBinding);
  }
#endif

  RefPtr<nsXBLBinding> mBinding;
};

// Structure used for maintaining state information during the
// frame construction process
class MOZ_STACK_CLASS nsFrameConstructorState {
public:
  typedef nsIFrame::ChildListID ChildListID;

  nsPresContext            *mPresContext;
  nsIPresShell             *mPresShell;
  nsFrameManager           *mFrameManager;

#ifdef MOZ_XUL
  // Frames destined for the kPopupList.
  nsAbsoluteItems           mPopupItems;
#endif

  // Containing block information for out-of-flow frames.
  nsAbsoluteItems           mFixedItems;
  nsAbsoluteItems           mAbsoluteItems;
  nsAbsoluteItems           mFloatedItems;
  // The containing block of a frame in the top layer is defined by the
  // spec: fixed-positioned frames are children of the viewport frame,
  // and absolutely-positioned frames are children of the initial
  // containing block. They would not be caught by any other containing
  // block, e.g. frames with transform or filter.
  nsAbsoluteItems           mTopLayerFixedItems;
  nsAbsoluteItems           mTopLayerAbsoluteItems;

  nsCOMPtr<nsILayoutHistoryState> mFrameState;
  // These bits will be added to the state bits of any frame we construct
  // using this state.
  nsFrameState              mAdditionalStateBits;

  // When working with the transform and filter properties, we want to hook
  // the abs-pos and fixed-pos lists together, since such
  // elements are fixed-pos containing blocks.  This flag determines
  // whether or not we want to wire the fixed-pos and abs-pos lists
  // together.
  bool                      mFixedPosIsAbsPos;

  // A boolean to indicate whether we have a "pending" popupgroup.  That is, we
  // have already created the FrameConstructionItem for the root popupgroup but
  // we have not yet created the relevant frame.
  bool                      mHavePendingPopupgroup;

  // If false (which is the default) then call SetPrimaryFrame() as needed
  // during frame construction.  If true, don't make any SetPrimaryFrame()
  // calls, except for generated content which doesn't have a primary frame
  // yet.  The mCreatingExtraFrames == true mode is meant to be used for
  // construction of random "extra" frames for elements via normal frame
  // construction APIs (e.g. replication of things across pages in paginated
  // mode).
  bool                      mCreatingExtraFrames;

  nsCOMArray<nsIContent>    mGeneratedTextNodesWithInitializer;

  TreeMatchContext          mTreeMatchContext;

  // Constructor
  // Use the passed-in history state.
  nsFrameConstructorState(
    nsIPresShell* aPresShell,
    nsContainerFrame* aFixedContainingBlock,
    nsContainerFrame* aAbsoluteContainingBlock,
    nsContainerFrame* aFloatContainingBlock,
    already_AddRefed<nsILayoutHistoryState> aHistoryState);
  // Get the history state from the pres context's pres shell.
  nsFrameConstructorState(nsIPresShell*          aPresShell,
                          nsContainerFrame*      aFixedContainingBlock,
                          nsContainerFrame*      aAbsoluteContainingBlock,
                          nsContainerFrame*      aFloatContainingBlock);

  ~nsFrameConstructorState();

  // Function to push the existing absolute containing block state and
  // create a new scope. Code that uses this function should get matching
  // logic in GetAbsoluteContainingBlock.
  // Also makes aNewAbsoluteContainingBlock the containing block for
  // fixed-pos elements if necessary.
  // aPositionedFrame is the frame whose style actually makes
  // aNewAbsoluteContainingBlock a containing block. E.g. for a scrollable element
  // aPositionedFrame is the element's primary frame and
  // aNewAbsoluteContainingBlock is the scrolled frame.
  void PushAbsoluteContainingBlock(nsContainerFrame* aNewAbsoluteContainingBlock,
                                   nsIFrame* aPositionedFrame,
                                   nsFrameConstructorSaveState& aSaveState);

  // Function to push the existing float containing block state and
  // create a new scope. Code that uses this function should get matching
  // logic in GetFloatContainingBlock.
  // Pushing a null float containing block forbids any frames from being
  // floated until a new float containing block is pushed.
  // XXX we should get rid of null float containing blocks and teach the
  // various frame classes to deal with floats instead.
  void PushFloatContainingBlock(nsContainerFrame* aNewFloatContainingBlock,
                                nsFrameConstructorSaveState& aSaveState);

  // Function to return the proper geometric parent for a frame with display
  // struct given by aStyleDisplay and parent's frame given by
  // aContentParentFrame.
  nsContainerFrame* GetGeometricParent(const nsStyleDisplay* aStyleDisplay,
                                       nsContainerFrame* aContentParentFrame) const;

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
   */
  void AddChild(nsIFrame* aNewFrame,
                nsFrameItems& aFrameItems,
                nsIContent* aContent,
                nsStyleContext* aStyleContext,
                nsContainerFrame* aParentFrame,
                bool aCanBePositioned = true,
                bool aCanBeFloated = true,
                bool aIsOutOfFlowPopup = false,
                bool aInsertAfter = false,
                nsIFrame* aInsertAfterFrame = nullptr);

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


  /**
   * class to automatically push and pop a pending binding in the frame
   * constructor state.  See nsCSSFrameConstructor::FrameConstructionItem
   * mPendingBinding documentation.
   */
  class PendingBindingAutoPusher;
  friend class PendingBindingAutoPusher;
  class MOZ_STACK_CLASS PendingBindingAutoPusher {
  public:
    PendingBindingAutoPusher(nsFrameConstructorState& aState,
                             PendingBinding* aPendingBinding) :
      mState(aState),
      mPendingBinding(aState.mCurrentPendingBindingInsertionPoint)
        {
          if (aPendingBinding) {
            aState.mCurrentPendingBindingInsertionPoint = aPendingBinding;
          }
        }

    ~PendingBindingAutoPusher()
      {
        mState.mCurrentPendingBindingInsertionPoint = mPendingBinding;
      }

  private:
    nsFrameConstructorState& mState;
    PendingBinding* mPendingBinding;
  };

  /**
   * Add a new pending binding to the list
   */
  void AddPendingBinding(PendingBinding* aPendingBinding) {
    if (mCurrentPendingBindingInsertionPoint) {
      mCurrentPendingBindingInsertionPoint->setPrevious(aPendingBinding);
    } else {
      mPendingBindings.insertBack(aPendingBinding);
    }
  }

protected:
  friend class nsFrameConstructorSaveState;

  /**
   * ProcessFrameInsertions takes the frames in aFrameItems and adds them as
   * kids to the aChildListID child list of |aFrameItems.containingBlock|.
   */
  void ProcessFrameInsertions(nsAbsoluteItems& aFrameItems,
                              ChildListID aChildListID);

  /**
   * GetOutOfFlowFrameItems selects the out-of-flow frame list the new
   * frame should be added to. If the frame shouldn't be added to any
   * out-of-flow list, it returns nullptr. The corresponding type of
   * placeholder is also returned via the aPlaceholderType parameter
   * if this method doesn't return nullptr. The caller should check
   * whether the returned list really has a containing block.
   */
  nsAbsoluteItems* GetOutOfFlowFrameItems(nsIFrame* aNewFrame,
                                          bool aCanBePositioned,
                                          bool aCanBeFloated,
                                          bool aIsOutOfFlowPopup,
                                          nsFrameState* aPlaceholderType);

  void ConstructBackdropFrameFor(nsIContent* aContent, nsIFrame* aFrame);

  // Our list of all pending bindings.  When we're done, we need to call
  // AddToAttachedQueue on all of them, in order.
  LinkedList<PendingBinding> mPendingBindings;

  PendingBinding* mCurrentPendingBindingInsertionPoint;
};

nsFrameConstructorState::nsFrameConstructorState(
  nsIPresShell* aPresShell,
  nsContainerFrame* aFixedContainingBlock,
  nsContainerFrame* aAbsoluteContainingBlock,
  nsContainerFrame* aFloatContainingBlock,
  already_AddRefed<nsILayoutHistoryState> aHistoryState)
  : mPresContext(aPresShell->GetPresContext()),
    mPresShell(aPresShell),
    mFrameManager(aPresShell->FrameManager()),
#ifdef MOZ_XUL
    mPopupItems(nullptr),
#endif
    mFixedItems(aFixedContainingBlock),
    mAbsoluteItems(aAbsoluteContainingBlock),
    mFloatedItems(aFloatContainingBlock),
    mTopLayerFixedItems(
      static_cast<nsContainerFrame*>(mFrameManager->GetRootFrame())),
    mTopLayerAbsoluteItems(
      aPresShell->FrameConstructor()->GetDocElementContainingBlock()),
    // See PushAbsoluteContaningBlock below
    mFrameState(aHistoryState),
    mAdditionalStateBits(nsFrameState(0)),
    // If the fixed-pos containing block is equal to the abs-pos containing
    // block, use the abs-pos containing block's abs-pos list for fixed-pos
    // frames.
    mFixedPosIsAbsPos(aFixedContainingBlock == aAbsoluteContainingBlock),
    mHavePendingPopupgroup(false),
    mCreatingExtraFrames(false),
    mTreeMatchContext(true, nsRuleWalker::eRelevantLinkUnvisited,
                      aPresShell->GetDocument()),
    mCurrentPendingBindingInsertionPoint(nullptr)
{
#ifdef MOZ_XUL
  nsIRootBox* rootBox = nsIRootBox::GetRootBox(aPresShell);
  if (rootBox) {
    mPopupItems.containingBlock = rootBox->GetPopupSetFrame();
  }
#endif
  MOZ_COUNT_CTOR(nsFrameConstructorState);
}

nsFrameConstructorState::nsFrameConstructorState(nsIPresShell* aPresShell,
                                                 nsContainerFrame* aFixedContainingBlock,
                                                 nsContainerFrame* aAbsoluteContainingBlock,
                                                 nsContainerFrame* aFloatContainingBlock)
  : nsFrameConstructorState(aPresShell, aFixedContainingBlock,
                            aAbsoluteContainingBlock,
                            aFloatContainingBlock,
                            aPresShell->GetDocument()->GetLayoutHistoryState())
{
}

nsFrameConstructorState::~nsFrameConstructorState()
{
  MOZ_COUNT_DTOR(nsFrameConstructorState);
  ProcessFrameInsertions(mTopLayerFixedItems, nsIFrame::kFixedList);
  ProcessFrameInsertions(mTopLayerAbsoluteItems, nsIFrame::kAbsoluteList);
  ProcessFrameInsertions(mFloatedItems, nsIFrame::kFloatList);
  ProcessFrameInsertions(mAbsoluteItems, nsIFrame::kAbsoluteList);
  ProcessFrameInsertions(mFixedItems, nsIFrame::kFixedList);
#ifdef MOZ_XUL
  ProcessFrameInsertions(mPopupItems, nsIFrame::kPopupList);
#endif
  for (int32_t i = mGeneratedTextNodesWithInitializer.Count() - 1; i >= 0; --i) {
    mGeneratedTextNodesWithInitializer[i]->
      DeleteProperty(nsGkAtoms::genConInitializerProperty);
  }
  if (!mPendingBindings.isEmpty()) {
    nsBindingManager* bindingManager = mPresShell->GetDocument()->BindingManager();
    do {
      nsAutoPtr<PendingBinding> pendingBinding;
      pendingBinding = mPendingBindings.popFirst();
      bindingManager->AddToAttachedQueue(pendingBinding->mBinding);
    } while (!mPendingBindings.isEmpty());
    mCurrentPendingBindingInsertionPoint = nullptr;
  }
}

static nsContainerFrame*
AdjustAbsoluteContainingBlock(nsContainerFrame* aContainingBlockIn)
{
  if (!aContainingBlockIn) {
    return nullptr;
  }

  // Always use the container's first continuation. (Inline frames can have
  // non-fluid bidi continuations...)
  return static_cast<nsContainerFrame*>(aContainingBlockIn->FirstContinuation());
}

void
nsFrameConstructorState::PushAbsoluteContainingBlock(nsContainerFrame* aNewAbsoluteContainingBlock,
                                                     nsIFrame* aPositionedFrame,
                                                     nsFrameConstructorSaveState& aSaveState)
{
  aSaveState.mItems = &mAbsoluteItems;
  aSaveState.mSavedItems = mAbsoluteItems;
  aSaveState.mChildListID = nsIFrame::kAbsoluteList;
  aSaveState.mState = this;
  aSaveState.mSavedFixedPosIsAbsPos = mFixedPosIsAbsPos;

  if (mFixedPosIsAbsPos) {
    // Since we're going to replace mAbsoluteItems, we need to save it into
    // mFixedItems now (and save the current value of mFixedItems).
    aSaveState.mSavedFixedItems = mFixedItems;
    mFixedItems = mAbsoluteItems;
  }

  mAbsoluteItems =
    nsAbsoluteItems(AdjustAbsoluteContainingBlock(aNewAbsoluteContainingBlock));

  /* See if we're wiring the fixed-pos and abs-pos lists together.  This happens iff
   * we're a transformed element.
   */
  mFixedPosIsAbsPos = aPositionedFrame &&
      aPositionedFrame->IsFixedPosContainingBlock();

  if (aNewAbsoluteContainingBlock) {
    aNewAbsoluteContainingBlock->MarkAsAbsoluteContainingBlock();
  }
}

void
nsFrameConstructorState::PushFloatContainingBlock(nsContainerFrame* aNewFloatContainingBlock,
                                                  nsFrameConstructorSaveState& aSaveState)
{
  NS_PRECONDITION(!aNewFloatContainingBlock ||
                  aNewFloatContainingBlock->IsFloatContainingBlock(),
                  "Please push a real float containing block!");
  NS_ASSERTION(!aNewFloatContainingBlock ||
               !ShouldSuppressFloatingOfDescendants(aNewFloatContainingBlock),
               "We should not push a frame that is supposed to _suppress_ "
               "floats as a float containing block!");
  aSaveState.mItems = &mFloatedItems;
  aSaveState.mSavedItems = mFloatedItems;
  aSaveState.mChildListID = nsIFrame::kFloatList;
  aSaveState.mState = this;
  mFloatedItems = nsAbsoluteItems(aNewFloatContainingBlock);
}

nsContainerFrame*
nsFrameConstructorState::GetGeometricParent(const nsStyleDisplay* aStyleDisplay,
                                            nsContainerFrame* aContentParentFrame) const
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
  // tables and table wrappers which share the same content). (2) requires some
  // work and possible factoring.
  //
  // XXXbz couldn't we just force position to "static" on roots and
  // float to "none"?  That's OK per CSS 2.1, as far as I can tell.

  if (aContentParentFrame && aContentParentFrame->IsSVGText()) {
    return aContentParentFrame;
  }

  if (aStyleDisplay->IsFloatingStyle() && mFloatedItems.containingBlock) {
    NS_ASSERTION(!aStyleDisplay->IsAbsolutelyPositionedStyle(),
                 "Absolutely positioned _and_ floating?");
    return mFloatedItems.containingBlock;
  }

  if (aStyleDisplay->mTopLayer != NS_STYLE_TOP_LAYER_NONE) {
    MOZ_ASSERT(aStyleDisplay->mTopLayer == NS_STYLE_TOP_LAYER_TOP,
               "-moz-top-layer should be either none or top");
    MOZ_ASSERT(aStyleDisplay->IsAbsolutelyPositionedStyle(),
               "Top layer items should always be absolutely positioned");
    if (aStyleDisplay->mPosition == NS_STYLE_POSITION_FIXED) {
      MOZ_ASSERT(mTopLayerFixedItems.containingBlock, "No root frame?");
      return mTopLayerFixedItems.containingBlock;
    }
    MOZ_ASSERT(aStyleDisplay->mPosition == NS_STYLE_POSITION_ABSOLUTE);
    MOZ_ASSERT(mTopLayerAbsoluteItems.containingBlock);
    return mTopLayerAbsoluteItems.containingBlock;
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

nsAbsoluteItems*
nsFrameConstructorState::GetOutOfFlowFrameItems(nsIFrame* aNewFrame,
                                                bool aCanBePositioned,
                                                bool aCanBeFloated,
                                                bool aIsOutOfFlowPopup,
                                                nsFrameState* aPlaceholderType)
{
#ifdef MOZ_XUL
  if (MOZ_UNLIKELY(aIsOutOfFlowPopup)) {
    MOZ_ASSERT(mPopupItems.containingBlock, "Must have a popup set frame!");
    *aPlaceholderType = PLACEHOLDER_FOR_POPUP;
    return &mPopupItems;
  }
#endif // MOZ_XUL
  if (aCanBeFloated && aNewFrame->IsFloating()) {
    *aPlaceholderType = PLACEHOLDER_FOR_FLOAT;
    return &mFloatedItems;
  }

  if (aCanBePositioned) {
    const nsStyleDisplay* disp = aNewFrame->StyleDisplay();
    if (disp->mTopLayer != NS_STYLE_TOP_LAYER_NONE) {
      *aPlaceholderType = PLACEHOLDER_FOR_TOPLAYER;
      if (disp->mPosition == NS_STYLE_POSITION_FIXED) {
        return &mTopLayerFixedItems;
      }
      return &mTopLayerAbsoluteItems;
    }
    if (disp->mPosition == NS_STYLE_POSITION_ABSOLUTE) {
      *aPlaceholderType = PLACEHOLDER_FOR_ABSPOS;
      return &mAbsoluteItems;
    }
    if (disp->mPosition == NS_STYLE_POSITION_FIXED) {
      *aPlaceholderType = PLACEHOLDER_FOR_FIXEDPOS;
      return &GetFixedItems();
    }
  }
  return nullptr;
}

void
nsFrameConstructorState::ConstructBackdropFrameFor(nsIContent* aContent,
                                                   nsIFrame* aFrame)
{
  MOZ_ASSERT(aFrame->StyleDisplay()->mTopLayer == NS_STYLE_TOP_LAYER_TOP);
  nsContainerFrame* frame = do_QueryFrame(aFrame);
  if (!frame) {
    NS_WARNING("Cannot create backdrop frame for non-container frame");
    return;
  }

  RefPtr<nsStyleContext> style = mPresShell->StyleSet()->
    ResolvePseudoElementStyle(aContent->AsElement(),
                              CSSPseudoElementType::backdrop,
                              /* aParentStyleContext */ nullptr,
                              /* aPseudoElement */ nullptr);
  MOZ_ASSERT(style->StyleDisplay()->mTopLayer == NS_STYLE_TOP_LAYER_TOP);
  nsContainerFrame* parentFrame =
    GetGeometricParent(style->StyleDisplay(), nullptr);

  nsBackdropFrame* backdropFrame = new (mPresShell) nsBackdropFrame(style);
  backdropFrame->Init(aContent, parentFrame, nullptr);

  nsFrameState placeholderType;
  nsAbsoluteItems* frameItems = GetOutOfFlowFrameItems(backdropFrame,
                                                       true, true, false,
                                                       &placeholderType);
  MOZ_ASSERT(placeholderType == PLACEHOLDER_FOR_TOPLAYER);

  nsIFrame* placeholder = nsCSSFrameConstructor::
    CreatePlaceholderFrameFor(mPresShell, aContent, backdropFrame,
                              frame->StyleContext(), frame, nullptr,
                              PLACEHOLDER_FOR_TOPLAYER);
  nsFrameList temp(placeholder, placeholder);
  frame->SetInitialChildList(nsIFrame::kBackdropList, temp);

  frameItems->AddChild(backdropFrame);
}

void
nsFrameConstructorState::AddChild(nsIFrame* aNewFrame,
                                  nsFrameItems& aFrameItems,
                                  nsIContent* aContent,
                                  nsStyleContext* aStyleContext,
                                  nsContainerFrame* aParentFrame,
                                  bool aCanBePositioned,
                                  bool aCanBeFloated,
                                  bool aIsOutOfFlowPopup,
                                  bool aInsertAfter,
                                  nsIFrame* aInsertAfterFrame)
{
  NS_PRECONDITION(!aNewFrame->GetNextSibling(), "Shouldn't happen");

  nsFrameState placeholderType;
  nsAbsoluteItems* outOfFlowFrameItems =
    GetOutOfFlowFrameItems(aNewFrame, aCanBePositioned, aCanBeFloated,
                           aIsOutOfFlowPopup, &placeholderType);

  // The comments in GetGeometricParent regarding root table frames
  // all apply here, unfortunately. Thus, we need to check whether
  // the returned frame items really has containing block.
  nsFrameItems* frameItems;
  if (outOfFlowFrameItems && outOfFlowFrameItems->containingBlock) {
    MOZ_ASSERT(aNewFrame->GetParent() == outOfFlowFrameItems->containingBlock,
               "Parent of the frame is not the containing block?");
    frameItems = outOfFlowFrameItems;
  } else {
    frameItems = &aFrameItems;
    placeholderType = nsFrameState(0);
  }

  if (placeholderType) {
    NS_ASSERTION(frameItems != &aFrameItems,
                 "Putting frame in-flow _and_ want a placeholder?");
    nsStyleContext* parentContext = aStyleContext->GetParent();
    nsIFrame* placeholderFrame =
      nsCSSFrameConstructor::CreatePlaceholderFrameFor(mPresShell,
                                                       aContent,
                                                       aNewFrame,
                                                       parentContext,
                                                       aParentFrame,
                                                       nullptr,
                                                       placeholderType);

    placeholderFrame->AddStateBits(mAdditionalStateBits);
    // Add the placeholder frame to the flow
    aFrameItems.AddChild(placeholderFrame);

    if (placeholderType == PLACEHOLDER_FOR_TOPLAYER) {
      ConstructBackdropFrameFor(aContent, aNewFrame);
    }
  }
#ifdef DEBUG
  else {
    NS_ASSERTION(aNewFrame->GetParent() == aParentFrame,
                 "In-flow frame has wrong parent");
  }
#endif

  if (aInsertAfter) {
    frameItems->InsertFrame(nullptr, aInsertAfterFrame, aNewFrame);
  } else {
    frameItems->AddChild(aNewFrame);
  }
}

void
nsFrameConstructorState::ProcessFrameInsertions(nsAbsoluteItems& aFrameItems,
                                                ChildListID aChildListID)
{
#define NS_NONXUL_LIST_TEST (&aFrameItems == &mFloatedItems &&            \
                             aChildListID == nsIFrame::kFloatList)    ||  \
                            ((&aFrameItems == &mAbsoluteItems ||          \
                              &aFrameItems == &mTopLayerAbsoluteItems) && \
                             aChildListID == nsIFrame::kAbsoluteList) ||  \
                            ((&aFrameItems == &mFixedItems ||             \
                              &aFrameItems == &mTopLayerFixedItems) &&    \
                             aChildListID == nsIFrame::kFixedList)
#ifdef MOZ_XUL
  NS_PRECONDITION(NS_NONXUL_LIST_TEST ||
                  (&aFrameItems == &mPopupItems &&
                   aChildListID == nsIFrame::kPopupList),
                  "Unexpected aFrameItems/aChildListID combination");
#else
  NS_PRECONDITION(NS_NONXUL_LIST_TEST,
                  "Unexpected aFrameItems/aChildListID combination");
#endif

  if (aFrameItems.IsEmpty()) {
    return;
  }

  nsContainerFrame* containingBlock = aFrameItems.containingBlock;

  NS_ASSERTION(containingBlock,
               "Child list without containing block?");

  if (aChildListID == nsIFrame::kFixedList) {
    // Put this frame on the transformed-frame's abs-pos list instead, if
    // it has abs-pos children instead of fixed-pos children.
    aChildListID = containingBlock->GetAbsoluteListID();
  }

  // Insert the frames hanging out in aItems.  We can use SetInitialChildList()
  // if the containing block hasn't been reflowed yet (so NS_FRAME_FIRST_REFLOW
  // is set) and doesn't have any frames in the aChildListID child list yet.
  const nsFrameList& childList = containingBlock->GetChildList(aChildListID);
  if (childList.IsEmpty() &&
      (containingBlock->GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    // If we're injecting absolutely positioned frames, inject them on the
    // absolute containing block
    if (aChildListID == containingBlock->GetAbsoluteListID()) {
      containingBlock->GetAbsoluteContainingBlock()->
        SetInitialChildList(containingBlock, aChildListID, aFrameItems);
    } else {
      containingBlock->SetInitialChildList(aChildListID, aFrameItems);
    }
  } else if (aChildListID == nsIFrame::kFixedList ||
             aChildListID == nsIFrame::kAbsoluteList) {
    // The order is not important for abs-pos/fixed-pos frame list, just
    // append the frame items to the list directly.
    mFrameManager->AppendFrames(containingBlock, aChildListID, aFrameItems);
  } else {
    // Note that whether the frame construction context is doing an append or
    // not is not helpful here, since it could be appending to some frame in
    // the middle of the document, which means we're not necessarily
    // appending to the children of the containing block.
    //
    // We need to make sure the 'append to the end of document' case is fast.
    // So first test the last child of the containing block
    nsIFrame* lastChild = childList.LastChild();

    // CompareTreePosition uses placeholder hierarchy for out of flow frames,
    // so this will make out-of-flows respect the ordering of placeholders,
    // which is great because it takes care of anonymous content.
    nsIFrame* firstNewFrame = aFrameItems.FirstChild();

    // Cache the ancestor chain so that we can reuse it if needed.
    AutoTArray<nsIFrame*, 20> firstNewFrameAncestors;
    nsIFrame* notCommonAncestor = nullptr;
    if (lastChild) {
      notCommonAncestor = nsLayoutUtils::FillAncestors(firstNewFrame,
                                                       containingBlock,
                                                       &firstNewFrameAncestors);
    }

    if (!lastChild ||
        nsLayoutUtils::CompareTreePosition(lastChild, firstNewFrame,
                                           firstNewFrameAncestors,
                                           notCommonAncestor ?
                                             containingBlock : nullptr) < 0) {
      // no lastChild, or lastChild comes before the new children, so just append
      mFrameManager->AppendFrames(containingBlock, aChildListID, aFrameItems);
    } else {
      // Try the other children. First collect them to an array so that a
      // reasonable fast binary search can be used to find the insertion point.
      AutoTArray<nsIFrame*, 128> children;
      for (nsIFrame* f = childList.FirstChild(); f != lastChild;
           f = f->GetNextSibling()) {
        children.AppendElement(f);
      }

      nsIFrame* insertionPoint = nullptr;
      int32_t imin = 0;
      int32_t max = children.Length();
      while (max > imin) {
        int32_t imid = imin + ((max - imin) / 2);
        nsIFrame* f = children[imid];
        int32_t compare =
          nsLayoutUtils::CompareTreePosition(f, firstNewFrame, firstNewFrameAncestors,
                                             notCommonAncestor ? containingBlock : nullptr);
        if (compare > 0) {
          // f is after the new frame.
          max = imid;
          insertionPoint = imid > 0 ? children[imid - 1] : nullptr;
        } else if (compare < 0) {
          // f is before the new frame.
          imin = imid + 1;
          insertionPoint = f;
        } else {
          // This is for the old behavior. Should be removed once it is
          // guaranteed that CompareTreePosition can't return 0!
          // See bug 928645.
          NS_WARNING("Something odd happening???");
          insertionPoint = nullptr;
          for (uint32_t i = 0; i < children.Length(); ++i) {
            nsIFrame* f = children[i];
            if (nsLayoutUtils::CompareTreePosition(f, firstNewFrame,
                                                   firstNewFrameAncestors,
                                                   notCommonAncestor ?
                                                     containingBlock : nullptr) > 0) {
              break;
            }
            insertionPoint = f;
          }
          break;
        }
      }
      mFrameManager->InsertFrames(containingBlock, aChildListID,
                                  insertionPoint, aFrameItems);
    }
  }

  NS_POSTCONDITION(aFrameItems.IsEmpty(), "How did that happen?");
}


nsFrameConstructorSaveState::nsFrameConstructorSaveState()
  : mItems(nullptr),
    mSavedItems(nullptr),
    mChildListID(kPrincipalList),
    mState(nullptr),
    mSavedFixedItems(nullptr),
    mSavedFixedPosIsAbsPos(false)
{
}

nsFrameConstructorSaveState::~nsFrameConstructorSaveState()
{
  // Restore the state
  if (mItems) {
    NS_ASSERTION(mState, "Can't have mItems set without having a state!");
    mState->ProcessFrameInsertions(*mItems, mChildListID);
    *mItems = mSavedItems;
#ifdef DEBUG
    // We've transferred the child list, so drop the pointer we held to it.
    // Note that this only matters for the assert in ~nsAbsoluteItems.
    mSavedItems.Clear();
#endif
    if (mItems == &mState->mAbsoluteItems) {
      mState->mFixedPosIsAbsPos = mSavedFixedPosIsAbsPos;
      if (mSavedFixedPosIsAbsPos) {
        // mAbsoluteItems was moved to mFixedItems, so move mFixedItems back
        // and repair the old mFixedItems now.
        mState->mAbsoluteItems = mState->mFixedItems;
        mState->mFixedItems = mSavedFixedItems;
#ifdef DEBUG
        mSavedFixedItems.Clear();
#endif
      }
    }
    NS_ASSERTION(!mItems->LastChild() || !mItems->LastChild()->GetNextSibling(),
                 "Something corrupted our list");
  }
}

/**
 * Moves aFrameList from aOldParent to aNewParent.  This updates the parent
 * pointer of the frames in the list, and reparents their views as needed.
 * nsFrame::SetParent sets the NS_FRAME_HAS_VIEW bit on aNewParent and its
 * ancestors as needed. Then it sets the list as the initial child list
 * on aNewParent, unless aNewParent either already has kids or has been
 * reflowed; in that case it appends the new frames.  Note that this
 * method differs from ReparentFrames in that it doesn't change the kids'
 * style contexts.
 */
// XXXbz Since this is only used for {ib} splits, could we just copy the view
// bits from aOldParent to aNewParent and then use the
// nsFrameList::ApplySetParent?  That would still leave us doing two passes
// over the list, of course; if we really wanted to we could factor out the
// relevant part of ReparentFrameViewList, I suppose...  Or just get rid of
// views, which would make most of this function go away.
static void
MoveChildrenTo(nsIFrame* aOldParent,
               nsContainerFrame* aNewParent,
               nsFrameList& aFrameList)
{
  bool sameGrandParent = aOldParent->GetParent() == aNewParent->GetParent();

  if (aNewParent->HasView() || aOldParent->HasView() || !sameGrandParent) {
    // Move the frames into the new view
    nsContainerFrame::ReparentFrameViewList(aFrameList, aOldParent, aNewParent);
  }

  for (nsFrameList::Enumerator e(aFrameList); !e.AtEnd(); e.Next()) {
    e.get()->SetParent(aNewParent);
  }

  if (aNewParent->PrincipalChildList().IsEmpty() &&
      (aNewParent->GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    aNewParent->SetInitialChildList(kPrincipalList, aFrameList);
  } else {
    aNewParent->AppendFrames(kPrincipalList, aFrameList);
  }
}

//----------------------------------------------------------------------

nsCSSFrameConstructor::nsCSSFrameConstructor(nsIDocument* aDocument,
                                             nsIPresShell* aPresShell)
  : nsFrameManager(aPresShell)
  , mDocument(aDocument)
  , mRootElementFrame(nullptr)
  , mRootElementStyleFrame(nullptr)
  , mDocElementContainingBlock(nullptr)
  , mGfxScrollFrame(nullptr)
  , mPageSequenceFrame(nullptr)
  , mCurrentDepth(0)
#ifdef DEBUG
  , mUpdateCount(0)
#endif
  , mQuotesDirty(false)
  , mCountersDirty(false)
  , mIsDestroyingFrameTree(false)
  , mHasRootAbsPosContainingBlock(false)
  , mAlwaysCreateFramesForIgnorableWhitespace(false)
{
#ifdef DEBUG
  static bool gFirstTime = true;
  if (gFirstTime) {
    gFirstTime = false;
    char* flags = PR_GetEnv("GECKO_FRAMECTOR_DEBUG_FLAGS");
    if (flags) {
      bool error = false;
      for (;;) {
        char* comma = PL_strchr(flags, ',');
        if (comma)
          *comma = '\0';

        bool found = false;
        FrameCtorDebugFlags* flag = gFlags;
        FrameCtorDebugFlags* limit = gFlags + NUM_DEBUG_FLAGS;
        while (flag < limit) {
          if (PL_strcasecmp(flag->name, flags) == 0) {
            *(flag->on) = true;
            printf("nsCSSFrameConstructor: setting %s debug flag on\n", flag->name);
            found = true;
            break;
          }
          ++flag;
        }

        if (! found)
          error = true;

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

  RestyleManager()->NotifyDestroyingFrame(aFrame);

  nsFrameManager::NotifyDestroyingFrame(aFrame);
}

struct nsGenConInitializer {
  nsAutoPtr<nsGenConNode> mNode;
  nsGenConList*           mList;
  void (nsCSSFrameConstructor::*mDirtyAll)();

  nsGenConInitializer(nsGenConNode* aNode, nsGenConList* aList,
                      void (nsCSSFrameConstructor::*aDirtyAll)())
    : mNode(aNode), mList(aList), mDirtyAll(aDirtyAll) {}
};

already_AddRefed<nsIContent>
nsCSSFrameConstructor::CreateGenConTextNode(nsFrameConstructorState& aState,
                                            const nsString& aString,
                                            RefPtr<nsTextNode>* aText,
                                            nsGenConInitializer* aInitializer)
{
  RefPtr<nsTextNode> content = new nsTextNode(mDocument->NodeInfoManager());
  content->SetText(aString, false);
  if (aText) {
    *aText = content;
  }
  if (aInitializer) {
    content->SetProperty(nsGkAtoms::genConInitializerProperty, aInitializer,
                         nsINode::DeleteProperty<nsGenConInitializer>);
    aState.mGeneratedTextNodesWithInitializer.AppendObject(content);
  }
  return content.forget();
}

already_AddRefed<nsIContent>
nsCSSFrameConstructor::CreateGeneratedContent(nsFrameConstructorState& aState,
                                              nsIContent*     aParentContent,
                                              nsStyleContext* aStyleContext,
                                              uint32_t        aContentIndex)
{
  // Get the content value
  const nsStyleContentData &data =
    aStyleContext->StyleContent()->ContentAt(aContentIndex);
  nsStyleContentType type = data.mType;

  if (eStyleContentType_Image == type) {
    if (!data.mContent.mImage) {
      // CSS had something specified that couldn't be converted to an
      // image object
      return nullptr;
    }

    // Create an image content object and pass it the image request.
    // XXX Check if it's an image type we can handle...

    RefPtr<NodeInfo> nodeInfo;
    nodeInfo = mDocument->NodeInfoManager()->
      GetNodeInfo(nsGkAtoms::mozgeneratedcontentimage, nullptr,
                  kNameSpaceID_XHTML, nsIDOMNode::ELEMENT_NODE);

    nsCOMPtr<nsIContent> content;
    NS_NewGenConImageContent(getter_AddRefs(content), nodeInfo.forget(),
                             data.mContent.mImage);
    return content.forget();
  }

  switch (type) {
  case eStyleContentType_String:
    return CreateGenConTextNode(aState,
                                nsDependentString(data.mContent.mString),
                                nullptr, nullptr);

  case eStyleContentType_Attr:
    {
      nsCOMPtr<nsIAtom> attrName;
      int32_t attrNameSpace = kNameSpaceID_None;
      nsAutoString contentString(data.mContent.mString);

      int32_t barIndex = contentString.FindChar('|'); // CSS namespace delimiter
      if (-1 != barIndex) {
        nsAutoString  nameSpaceVal;
        contentString.Left(nameSpaceVal, barIndex);
        nsresult error;
        attrNameSpace = nameSpaceVal.ToInteger(&error);
        contentString.Cut(0, barIndex + 1);
        if (contentString.Length()) {
          if (mDocument->IsHTMLDocument() && aParentContent->IsHTMLElement()) {
            ToLowerCase(contentString);
          }
          attrName = NS_Atomize(contentString);
        }
      }
      else {
        if (mDocument->IsHTMLDocument() && aParentContent->IsHTMLElement()) {
          ToLowerCase(contentString);
        }
        attrName = NS_Atomize(contentString);
      }

      if (!attrName) {
        return nullptr;
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

      nsCounterUseNode* node =
        new nsCounterUseNode(mPresShell->GetPresContext(),
                             counters, aContentIndex,
                             type == eStyleContentType_Counters);

      nsGenConInitializer* initializer =
        new nsGenConInitializer(node, counterList,
                                &nsCSSFrameConstructor::CountersDirty);
      return CreateGenConTextNode(aState, EmptyString(), &node->mText,
                                  initializer);
    }

  case eStyleContentType_Image:
    NS_NOTREACHED("handled by if above");
    return nullptr;

  case eStyleContentType_OpenQuote:
  case eStyleContentType_CloseQuote:
  case eStyleContentType_NoOpenQuote:
  case eStyleContentType_NoCloseQuote:
    {
      nsQuoteNode* node =
        new nsQuoteNode(type, aContentIndex);

      nsGenConInitializer* initializer =
        new nsGenConInitializer(node, &mQuoteList,
                                &nsCSSFrameConstructor::QuotesDirty);
      return CreateGenConTextNode(aState, EmptyString(), &node->mText,
                                  initializer);
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

      if (aParentContent->IsHTMLElement() &&
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
        return CreateGenConTextNode(aState, temp, nullptr, nullptr);
      }

      break;
    }

  case eStyleContentType_Uninitialized:
    NS_NOTREACHED("uninitialized content type");
    return nullptr;
  } // switch

  return nullptr;
}

/*
 * aParentFrame - the frame that should be the parent of the generated
 *   content.  This is the frame for the corresponding content node,
 *   which must not be a leaf frame.
 *
 * Any items created are added to aItems.
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
nsCSSFrameConstructor::CreateGeneratedContentItem(nsFrameConstructorState& aState,
                                                  nsContainerFrame* aParentFrame,
                                                  nsIContent*      aParentContent,
                                                  nsStyleContext*  aStyleContext,
                                                  CSSPseudoElementType aPseudoElement,
                                                  FrameConstructionItemList& aItems)
{
  MOZ_ASSERT(aPseudoElement == CSSPseudoElementType::before ||
             aPseudoElement == CSSPseudoElementType::after,
             "unexpected aPseudoElement");

  // XXXbz is this ever true?
  if (!aParentContent->IsElement()) {
    NS_ERROR("Bogus generated content parent");
    return;
  }

  StyleSetHandle styleSet = mPresShell->StyleSet();

  // Probe for the existence of the pseudo-element
  RefPtr<nsStyleContext> pseudoStyleContext;
  pseudoStyleContext =
    styleSet->ProbePseudoElementStyle(aParentContent->AsElement(),
                                      aPseudoElement,
                                      aStyleContext,
                                      aState.mTreeMatchContext);
  if (!pseudoStyleContext)
    return;

  bool isBefore = aPseudoElement == CSSPseudoElementType::before;

  // |ProbePseudoStyleFor| checked the 'display' property and the
  // |ContentCount()| of the 'content' property for us.
  RefPtr<NodeInfo> nodeInfo;
  nsIAtom* elemName = isBefore ?
    nsGkAtoms::mozgeneratedcontentbefore : nsGkAtoms::mozgeneratedcontentafter;
  nodeInfo = mDocument->NodeInfoManager()->GetNodeInfo(elemName, nullptr,
                                                       kNameSpaceID_None,
                                                       nsIDOMNode::ELEMENT_NODE);
  nsCOMPtr<Element> container;
  nsresult rv = NS_NewXMLElement(getter_AddRefs(container), nodeInfo.forget());
  if (NS_FAILED(rv))
    return;
  container->SetIsNativeAnonymousRoot();

  // If the parent is in a shadow tree, make sure we don't
  // bind with a document because shadow roots and its descendants
  // are not in document.
  nsIDocument* bindDocument =
    aParentContent->HasFlag(NODE_IS_IN_SHADOW_TREE) ? nullptr : mDocument;
  rv = container->BindToTree(bindDocument, aParentContent, aParentContent, true);
  if (NS_FAILED(rv)) {
    container->UnbindFromTree();
    return;
  }

  // stylo: ServoRestyleManager does not handle transitions yet, and when it
  // does it probably won't need to track reframed style contexts to start
  // transitions correctly.
  if (mozilla::RestyleManager* geckoRM = RestyleManager()->GetAsGecko()) {
    RestyleManager::ReframingStyleContexts* rsc =
      geckoRM->GetReframingStyleContexts();
    if (rsc) {
      nsStyleContext* oldStyleContext = rsc->Get(container, aPseudoElement);
      if (oldStyleContext) {
        RestyleManager::TryInitiatingTransition(aState.mPresContext,
                                                container,
                                                oldStyleContext,
                                                &pseudoStyleContext);
      } else {
        aState.mPresContext->TransitionManager()->
          PruneCompletedTransitions(container, aPseudoElement,
                                    pseudoStyleContext);
      }
    }
  }

  uint32_t contentCount = pseudoStyleContext->StyleContent()->ContentCount();
  for (uint32_t contentIndex = 0; contentIndex < contentCount; contentIndex++) {
    nsCOMPtr<nsIContent> content =
      CreateGeneratedContent(aState, aParentContent, pseudoStyleContext,
                             contentIndex);
    if (content) {
      container->AppendChildTo(content, false);
    }
  }

  AddFrameConstructionItemsInternal(aState, container, aParentFrame, elemName,
                                    kNameSpaceID_None, true,
                                    pseudoStyleContext,
                                    ITEM_IS_GENERATED_CONTENT, nullptr,
                                    aItems);
}

/****************************************************
 **  BEGIN TABLE SECTION
 ****************************************************/

// The term pseudo frame is being used instead of anonymous frame, since anonymous
// frame has been used elsewhere to refer to frames that have generated content

// Return whether the given frame is a table pseudo-frame. Note that
// cell-content and table-outer frames have pseudo-types, but are always
// created, even for non-anonymous cells and tables respectively.  So for those
// we have to examine the cell or table frame to see whether it's a pseudo
// frame. In particular, a lone table caption will have a table wrapper as its
// parent, but will also trigger construction of an empty inner table, which
// will be the one we can examine to see whether the wrapper was a pseudo-frame.
static bool
IsTablePseudo(nsIFrame* aFrame)
{
  nsIAtom* pseudoType = aFrame->StyleContext()->GetPseudo();
  return pseudoType &&
    (pseudoType == nsCSSAnonBoxes::table ||
     pseudoType == nsCSSAnonBoxes::inlineTable ||
     pseudoType == nsCSSAnonBoxes::tableColGroup ||
     pseudoType == nsCSSAnonBoxes::tableRowGroup ||
     pseudoType == nsCSSAnonBoxes::tableRow ||
     pseudoType == nsCSSAnonBoxes::tableCell ||
     (pseudoType == nsCSSAnonBoxes::cellContent &&
      aFrame->GetParent()->StyleContext()->GetPseudo() ==
        nsCSSAnonBoxes::tableCell) ||
     (pseudoType == nsCSSAnonBoxes::tableWrapper &&
      (aFrame->PrincipalChildList().FirstChild()->StyleContext()->GetPseudo() ==
         nsCSSAnonBoxes::table ||
       aFrame->PrincipalChildList().FirstChild()->StyleContext()->GetPseudo() ==
         nsCSSAnonBoxes::inlineTable)));
}

static bool
IsRubyPseudo(nsIFrame* aFrame)
{
  return RubyUtils::IsRubyPseudo(aFrame->StyleContext()->GetPseudo());
}

static bool
IsTableOrRubyPseudo(nsIFrame* aFrame)
{
  return IsTablePseudo(aFrame) || IsRubyPseudo(aFrame);
}

/* static */
nsCSSFrameConstructor::ParentType
nsCSSFrameConstructor::GetParentType(nsIAtom* aFrameType)
{
  if (aFrameType == nsGkAtoms::tableFrame) {
    return eTypeTable;
  }
  if (aFrameType == nsGkAtoms::tableRowGroupFrame) {
    return eTypeRowGroup;
  }
  if (aFrameType == nsGkAtoms::tableRowFrame) {
    return eTypeRow;
  }
  if (aFrameType == nsGkAtoms::tableColGroupFrame) {
    return eTypeColGroup;
  }
  if (aFrameType == nsGkAtoms::rubyBaseContainerFrame) {
    return eTypeRubyBaseContainer;
  }
  if (aFrameType == nsGkAtoms::rubyTextContainerFrame) {
    return eTypeRubyTextContainer;
  }
  if (aFrameType == nsGkAtoms::rubyFrame) {
    return eTypeRuby;
  }

  return eTypeBlock;
}

static nsContainerFrame*
AdjustCaptionParentFrame(nsContainerFrame* aParentFrame)
{
  if (nsGkAtoms::tableFrame == aParentFrame->GetType()) {
    return aParentFrame->GetParent();
  }
  return aParentFrame;
}

/**
 * If the parent frame is a |tableFrame| and the child is a
 * |captionFrame|, then we want to insert the frames beneath the
 * |tableFrame|'s parent frame. Returns |true| if the parent frame
 * needed to be fixed up.
 */
static bool
GetCaptionAdjustedParent(nsContainerFrame*  aParentFrame,
                         const nsIFrame*    aChildFrame,
                         nsContainerFrame** aAdjParentFrame)
{
  *aAdjParentFrame = aParentFrame;
  bool haveCaption = false;

  if (aChildFrame->IsTableCaption()) {
    haveCaption = true;
    *aAdjParentFrame = ::AdjustCaptionParentFrame(aParentFrame);
  }
  return haveCaption;
}

void
nsCSSFrameConstructor::AdjustParentFrame(nsContainerFrame**           aParentFrame,
                                         const FrameConstructionData* aFCData,
                                         nsStyleContext*              aStyleContext)
{
  NS_PRECONDITION(aStyleContext, "Must have child's style context");
  NS_PRECONDITION(aFCData, "Must have frame construction data");

  bool tablePart = ((aFCData->mBits & FCDATA_IS_TABLE_PART) != 0);

  if (tablePart && aStyleContext->StyleDisplay()->mDisplay ==
      StyleDisplay::TableCaption) {
    *aParentFrame = ::AdjustCaptionParentFrame(*aParentFrame);
  }
}

// Pull all the captions present in aItems out  into aCaptions
static void
PullOutCaptionFrames(nsFrameItems& aItems, nsFrameItems& aCaptions)
{
  nsIFrame* child = aItems.FirstChild();
  while (child) {
    nsIFrame* nextSibling = child->GetNextSibling();
    if (child->IsTableCaption()) {
      aItems.RemoveFrame(child);
      aCaptions.AddChild(child);
    }
    child = nextSibling;
  }
}


// Construct the outer, inner table frames and the children frames for the table.
// XXX Page break frames for pseudo table frames are not constructed to avoid the risk
// associated with revising the pseudo frame mechanism. The long term solution
// of having frames handle page-break-before/after will solve the problem.
nsIFrame*
nsCSSFrameConstructor::ConstructTable(nsFrameConstructorState& aState,
                                      FrameConstructionItem&   aItem,
                                      nsContainerFrame*        aParentFrame,
                                      const nsStyleDisplay*    aDisplay,
                                      nsFrameItems&            aFrameItems)
{
  NS_PRECONDITION(aDisplay->mDisplay == StyleDisplay::Table ||
                  aDisplay->mDisplay == StyleDisplay::InlineTable,
                  "Unexpected call");

  nsIContent* const content = aItem.mContent;
  nsStyleContext* const styleContext = aItem.mStyleContext;
  const uint32_t nameSpaceID = aItem.mNameSpaceID;

  // create the pseudo SC for the table wrapper as a child of the inner SC
  RefPtr<nsStyleContext> outerStyleContext;
  outerStyleContext = mPresShell->StyleSet()->
    ResolveAnonymousBoxStyle(nsCSSAnonBoxes::tableWrapper, styleContext);

  // Create the table wrapper frame which holds the caption and inner table frame
  nsContainerFrame* newFrame;
  if (kNameSpaceID_MathML == nameSpaceID)
    newFrame = NS_NewMathMLmtableOuterFrame(mPresShell, outerStyleContext);
  else
    newFrame = NS_NewTableWrapperFrame(mPresShell, outerStyleContext);

  nsContainerFrame* geometricParent =
    aState.GetGeometricParent(outerStyleContext->StyleDisplay(),
                              aParentFrame);

  // Init the table wrapper frame
  InitAndRestoreFrame(aState, content, geometricParent, newFrame);

  // Create the inner table frame
  nsContainerFrame* innerFrame;
  if (kNameSpaceID_MathML == nameSpaceID)
    innerFrame = NS_NewMathMLmtableFrame(mPresShell, styleContext);
  else
    innerFrame = NS_NewTableFrame(mPresShell, styleContext);

  InitAndRestoreFrame(aState, content, newFrame, innerFrame);

  // Put the newly created frames into the right child list
  SetInitialSingleChild(newFrame, innerFrame);

  aState.AddChild(newFrame, aFrameItems, content, styleContext, aParentFrame);

  if (!mRootElementFrame) {
    // The frame we're constructing will be the root element frame.
    // Set mRootElementFrame before processing children.
    mRootElementFrame = newFrame;
  }

  nsFrameItems childItems;

  // Process children
  nsFrameConstructorSaveState absoluteSaveState;
  const nsStyleDisplay* display = outerStyleContext->StyleDisplay();

  // Mark the table frame as an absolute container if needed
  newFrame->AddStateBits(NS_FRAME_CAN_HAVE_ABSPOS_CHILDREN);
  if (display->IsAbsPosContainingBlock(newFrame)) {
    aState.PushAbsoluteContainingBlock(newFrame, newFrame, absoluteSaveState);
  }
  NS_ASSERTION(aItem.mAnonChildren.IsEmpty(),
               "nsIAnonymousContentCreator::CreateAnonymousContent "
               "implementations for table frames are not currently expected "
               "to output a list where the items have their own children");
  if (aItem.mFCData->mBits & FCDATA_USE_CHILD_ITEMS) {
    ConstructFramesFromItemList(aState, aItem.mChildItems,
                                innerFrame, childItems);
  } else {
    ProcessChildren(aState, content, styleContext, innerFrame,
                    true, childItems, false, aItem.mPendingBinding);
  }

  nsFrameItems captionItems;
  PullOutCaptionFrames(childItems, captionItems);

  // Set the inner table frame's initial primary list
  innerFrame->SetInitialChildList(kPrincipalList, childItems);

  // Set the table wrapper frame's secondary childlist lists
  if (captionItems.NotEmpty()) {
    newFrame->SetInitialChildList(nsIFrame::kCaptionList, captionItems);
  }

  return newFrame;
}

static void
MakeTablePartAbsoluteContainingBlockIfNeeded(nsFrameConstructorState&     aState,
                                             const nsStyleDisplay*        aDisplay,
                                             nsFrameConstructorSaveState& aAbsSaveState,
                                             nsContainerFrame*            aFrame)
{
  // If we're positioned, then we need to become an absolute containing block
  // for any absolutely positioned children and register for post-reflow fixup.
  //
  // Note that usually if a frame type can be an absolute containing block, we
  // always set NS_FRAME_CAN_HAVE_ABSPOS_CHILDREN, whether it actually is or not.
  // However, in this case flag serves the additional purpose of indicating that
  // the frame was registered with its table frame. This allows us to avoid the
  // overhead of unregistering the frame in most cases.
  if (aDisplay->IsAbsPosContainingBlock(aFrame)) {
    aFrame->AddStateBits(NS_FRAME_CAN_HAVE_ABSPOS_CHILDREN);
    aState.PushAbsoluteContainingBlock(aFrame, aFrame, aAbsSaveState);
    nsTableFrame::RegisterPositionedTablePart(aFrame);
  }
}

nsIFrame*
nsCSSFrameConstructor::ConstructTableRowOrRowGroup(nsFrameConstructorState& aState,
                                                   FrameConstructionItem&   aItem,
                                                   nsContainerFrame*        aParentFrame,
                                                   const nsStyleDisplay*    aDisplay,
                                                   nsFrameItems&            aFrameItems)
{
  MOZ_ASSERT(aDisplay->mDisplay == StyleDisplay::TableRow ||
             aDisplay->mDisplay == StyleDisplay::TableRowGroup ||
             aDisplay->mDisplay == StyleDisplay::TableFooterGroup ||
             aDisplay->mDisplay == StyleDisplay::TableHeaderGroup,
             "Not a row or row group");
  MOZ_ASSERT(aItem.mStyleContext->StyleDisplay() == aDisplay,
             "Display style doesn't match style context");
  nsIContent* const content = aItem.mContent;
  nsStyleContext* const styleContext = aItem.mStyleContext;
  const uint32_t nameSpaceID = aItem.mNameSpaceID;

  nsContainerFrame* newFrame;
  if (aDisplay->mDisplay == StyleDisplay::TableRow) {
    if (kNameSpaceID_MathML == nameSpaceID)
      newFrame = NS_NewMathMLmtrFrame(mPresShell, styleContext);
    else
      newFrame = NS_NewTableRowFrame(mPresShell, styleContext);
  } else {
    newFrame = NS_NewTableRowGroupFrame(mPresShell, styleContext);
  }

  InitAndRestoreFrame(aState, content, aParentFrame, newFrame);

  nsFrameConstructorSaveState absoluteSaveState;
  MakeTablePartAbsoluteContainingBlockIfNeeded(aState, aDisplay,
                                               absoluteSaveState,
                                               newFrame);

  nsFrameItems childItems;
  NS_ASSERTION(aItem.mAnonChildren.IsEmpty(),
               "nsIAnonymousContentCreator::CreateAnonymousContent "
               "implementations for table frames are not currently expected "
               "to output a list where the items have their own children");
  if (aItem.mFCData->mBits & FCDATA_USE_CHILD_ITEMS) {
    ConstructFramesFromItemList(aState, aItem.mChildItems, newFrame,
                                childItems);
  } else {
    ProcessChildren(aState, content, styleContext, newFrame,
                    true, childItems, false, aItem.mPendingBinding);
  }

  newFrame->SetInitialChildList(kPrincipalList, childItems);
  aFrameItems.AddChild(newFrame);
  return newFrame;
}

nsIFrame*
nsCSSFrameConstructor::ConstructTableCol(nsFrameConstructorState& aState,
                                         FrameConstructionItem&   aItem,
                                         nsContainerFrame*        aParentFrame,
                                         const nsStyleDisplay*    aStyleDisplay,
                                         nsFrameItems&            aFrameItems)
{
  nsIContent* const content = aItem.mContent;
  nsStyleContext* const styleContext = aItem.mStyleContext;

  nsTableColFrame* colFrame = NS_NewTableColFrame(mPresShell, styleContext);
  InitAndRestoreFrame(aState, content, aParentFrame, colFrame);

  NS_ASSERTION(colFrame->StyleContext() == styleContext,
               "Unexpected style context");

  aFrameItems.AddChild(colFrame);

  // construct additional col frames if the col frame has a span > 1
  int32_t span = colFrame->GetSpan();
  for (int32_t spanX = 1; spanX < span; spanX++) {
    nsTableColFrame* newCol = NS_NewTableColFrame(mPresShell, styleContext);
    InitAndRestoreFrame(aState, content, aParentFrame, newCol, false);
    aFrameItems.LastChild()->SetNextContinuation(newCol);
    newCol->SetPrevContinuation(aFrameItems.LastChild());
    aFrameItems.AddChild(newCol);
    newCol->SetColType(eColAnonymousCol);
  }

  return colFrame;
}

nsIFrame*
nsCSSFrameConstructor::ConstructTableCell(nsFrameConstructorState& aState,
                                          FrameConstructionItem&   aItem,
                                          nsContainerFrame*        aParentFrame,
                                          const nsStyleDisplay*    aDisplay,
                                          nsFrameItems&            aFrameItems)
{
  MOZ_ASSERT(aDisplay->mDisplay == StyleDisplay::TableCell,
             "Unexpected call");

  nsIContent* const content = aItem.mContent;
  nsStyleContext* const styleContext = aItem.mStyleContext;
  const uint32_t nameSpaceID = aItem.mNameSpaceID;

  nsTableFrame* tableFrame =
    static_cast<nsTableRowFrame*>(aParentFrame)->GetTableFrame();
  nsContainerFrame* newFrame;
  // <mtable> is border separate in mathml.css and the MathML code doesn't implement
  // border collapse. For those users who style <mtable> with border collapse,
  // give them the default non-MathML table frames that understand border collapse.
  // This won't break us because MathML table frames are all subclasses of the default
  // table code, and so we can freely mix <mtable> with <mtr> or <tr>, <mtd> or <td>.
  // What will happen is just that non-MathML frames won't understand MathML attributes
  // and will therefore miss the special handling that the MathML code does.
  if (kNameSpaceID_MathML == nameSpaceID && !tableFrame->IsBorderCollapse()) {
    newFrame = NS_NewMathMLmtdFrame(mPresShell, styleContext, tableFrame);
  } else {
    // Warning: If you change this and add a wrapper frame around table cell
    // frames, make sure Bug 368554 doesn't regress!
    // See IsInAutoWidthTableCellForQuirk() in nsImageFrame.cpp.
    newFrame = NS_NewTableCellFrame(mPresShell, styleContext, tableFrame);
  }

  // Initialize the table cell frame
  InitAndRestoreFrame(aState, content, aParentFrame, newFrame);

  // Resolve pseudo style and initialize the body cell frame
  RefPtr<nsStyleContext> innerPseudoStyle;
  innerPseudoStyle = mPresShell->StyleSet()->
    ResolveAnonymousBoxStyle(nsCSSAnonBoxes::cellContent, styleContext);

  // Create a block frame that will format the cell's content
  bool isBlock;
  nsContainerFrame* cellInnerFrame;
  if (kNameSpaceID_MathML == nameSpaceID) {
    cellInnerFrame = NS_NewMathMLmtdInnerFrame(mPresShell, innerPseudoStyle);
    isBlock = false;
  } else {
    cellInnerFrame = NS_NewBlockFormattingContext(mPresShell, innerPseudoStyle);
    isBlock = true;
  }

  InitAndRestoreFrame(aState, content, newFrame, cellInnerFrame);

  nsFrameConstructorSaveState absoluteSaveState;
  MakeTablePartAbsoluteContainingBlockIfNeeded(aState, aDisplay,
                                               absoluteSaveState,
                                               newFrame);

  nsFrameItems childItems;
  NS_ASSERTION(aItem.mAnonChildren.IsEmpty(),
               "nsIAnonymousContentCreator::CreateAnonymousContent "
               "implementations for table frames are not currently expected "
               "to output a list where the items have their own children");
  if (aItem.mFCData->mBits & FCDATA_USE_CHILD_ITEMS) {
    // Need to push ourselves as a float containing block.
    // XXXbz it might be nice to work on getting the parent
    // FrameConstructionItem down into ProcessChildren and just making use of
    // the push there, but that's a bit of work.
    nsFrameConstructorSaveState floatSaveState;
    if (!isBlock) { /* MathML case */
      aState.PushFloatContainingBlock(nullptr, floatSaveState);
    } else {
      aState.PushFloatContainingBlock(cellInnerFrame, floatSaveState);
    }

    ConstructFramesFromItemList(aState, aItem.mChildItems, cellInnerFrame,
                                childItems);
  } else {
    // Process the child content
    ProcessChildren(aState, content, styleContext, cellInnerFrame,
                    true, childItems, isBlock, aItem.mPendingBinding);
  }

  cellInnerFrame->SetInitialChildList(kPrincipalList, childItems);
  SetInitialSingleChild(newFrame, cellInnerFrame);
  aFrameItems.AddChild(newFrame);
  return newFrame;
}

static inline bool
NeedFrameFor(const nsFrameConstructorState& aState,
             nsIFrame*   aParentFrame,
             nsIContent* aChildContent)
{
  // XXX the GetContent() != aChildContent check is needed due to bug 135040.
  // Remove it once that's fixed.
  NS_PRECONDITION(!aChildContent->GetPrimaryFrame() ||
                  aState.mCreatingExtraFrames ||
                  aChildContent->GetPrimaryFrame()->GetContent() != aChildContent,
                  "Why did we get called?");

  // don't create a whitespace frame if aParentFrame doesn't want it.
  // always create frames for children in generated content. counter(),
  // quotes, and attr() content can easily change dynamically and we don't
  // want to be reconstructing frames. It's not even clear that these
  // should be considered ignorable just because they evaluate to
  // whitespace.

  // We could handle all this in CreateNeededPseudoContainers or some other
  // place after we build our frame construction items, but that would involve
  // creating frame construction items for whitespace kids of
  // eExcludesIgnorableWhitespace frames, where we know we'll be dropping them
  // all anyway, and involve an extra walk down the frame construction item
  // list.
  if ((aParentFrame &&
       (!aParentFrame->IsFrameOfType(nsIFrame::eExcludesIgnorableWhitespace) ||
        aParentFrame->IsGeneratedContentFrame())) ||
      !aChildContent->IsNodeOfType(nsINode::eTEXT)) {
    return true;
  }

  aChildContent->SetFlags(NS_CREATE_FRAME_IF_NON_WHITESPACE |
                          NS_REFRAME_IF_WHITESPACE);
  return !aChildContent->TextIsOnlyWhitespace();
}

/***********************************************
 * END TABLE SECTION
 ***********************************************/

nsIFrame*
nsCSSFrameConstructor::ConstructDocElementFrame(Element*                 aDocElement,
                                                nsILayoutHistoryState*   aFrameState)
{
  MOZ_ASSERT(GetRootFrame(),
             "No viewport?  Someone forgot to call ConstructRootFrame!");
  MOZ_ASSERT(!mDocElementContainingBlock,
             "Shouldn't have a doc element containing block here");

  // Resolve a new style context for the viewport since it may be affected
  // by a new root element style (e.g. a propagated 'direction').
  // @see nsStyleContext::ApplyStyleFixups
  {
    RefPtr<nsStyleContext> sc = mPresShell->StyleSet()->
      ResolveAnonymousBoxStyle(nsCSSAnonBoxes::viewport, nullptr);
    GetRootFrame()->SetStyleContextWithoutNotification(sc);
  }

  // Make sure to call UpdateViewportScrollbarStylesOverride before
  // SetUpDocElementContainingBlock, since it sets up our scrollbar state
  // properly.
  DebugOnly<nsIContent*> propagatedScrollFrom;
  if (nsPresContext* presContext = mPresShell->GetPresContext()) {
    propagatedScrollFrom = presContext->UpdateViewportScrollbarStylesOverride();
  }

  SetUpDocElementContainingBlock(aDocElement);

  NS_ASSERTION(mDocElementContainingBlock, "Should have parent by now");

  nsFrameConstructorState state(mPresShell,
                                GetAbsoluteContainingBlock(mDocElementContainingBlock, FIXED_POS),
                                nullptr,
                                nullptr, do_AddRef(aFrameState));
  // Initialize the ancestor filter with null for now; we'll push
  // aDocElement once we finish resolving style for it.
  state.mTreeMatchContext.InitAncestors(nullptr);

  // XXXbz why, exactly?
  if (!mTempFrameTreeState)
    state.mPresShell->CaptureHistoryState(getter_AddRefs(mTempFrameTreeState));

  // Make sure that we'll handle restyles for this document element in
  // the future.  We need this, because the document element might
  // have stale restyle bits from a previous frame constructor for
  // this document.  Unlike in AddFrameConstructionItems, it's safe to
  // unset all element restyle flags, since we don't have any
  // siblings.
  aDocElement->UnsetRestyleFlagsIfGecko();

  // --------- CREATE AREA OR BOX FRAME -------
  // FIXME: Should this use ResolveStyleContext?  (The calls in this
  // function are the only case in nsCSSFrameConstructor where we don't
  // do so for the construction of a style context for an element.)
  RefPtr<nsStyleContext> styleContext;
  styleContext = mPresShell->StyleSet()->ResolveStyleFor(aDocElement,
                                                         nullptr,
                                                         ConsumeStyleBehavior::Consume,
                                                         LazyComputeBehavior::Allow);

  const nsStyleDisplay* display = styleContext->StyleDisplay();

  // Ensure that our XBL bindings are installed.
  if (display->mBinding) {
    // Get the XBL loader.
    nsresult rv;
    bool resolveStyle;

    nsXBLService* xblService = nsXBLService::GetInstance();
    if (!xblService) {
      return nullptr;
    }

    RefPtr<nsXBLBinding> binding;
    rv = xblService->LoadBindings(aDocElement, display->mBinding->GetURI(),
                                  display->mBinding->mOriginPrincipal,
                                  getter_AddRefs(binding), &resolveStyle);
    if (NS_FAILED(rv) && rv != NS_ERROR_XBL_BLOCKED)
      return nullptr; // Binding will load asynchronously.

    if (binding) {
      // For backwards compat, keep firing the root's constructor
      // after all of its kids' constructors.  So tell the binding
      // manager about it right now.
      mDocument->BindingManager()->AddToAttachedQueue(binding);
    }

    if (resolveStyle) {
      // FIXME: Should this use ResolveStyleContext?  (The calls in this
      // function are the only case in nsCSSFrameConstructor where we
      // don't do so for the construction of a style context for an
      // element.)
      styleContext = mPresShell->StyleSet()->ResolveStyleFor(aDocElement,
                                                             nullptr,
                                                             ConsumeStyleBehavior::Consume,
                                                             LazyComputeBehavior::Allow);
      display = styleContext->StyleDisplay();
    }
  }

  // We delay traversing the entire document until here, since we per above we
  // may invalidate the root style when we load doc stylesheets.
  if (ServoStyleSet* set = mPresShell->StyleSet()->GetAsServo()) {
    set->StyleDocument();
  }

  // --------- IF SCROLLABLE WRAP IN SCROLLFRAME --------

  NS_ASSERTION(!display->IsScrollableOverflow() ||
               state.mPresContext->IsPaginated() ||
               propagatedScrollFrom == aDocElement,
               "Scrollbars should have been propagated to the viewport");

  if (MOZ_UNLIKELY(display->mDisplay == StyleDisplay::None)) {
    SetUndisplayedContent(aDocElement, styleContext);
    return nullptr;
  }

  TreeMatchContext::AutoAncestorPusher ancestorPusher(state.mTreeMatchContext);
  ancestorPusher.PushAncestorAndStyleScope(aDocElement);

  // Make sure to start any background image loads for the root element now.
  styleContext->StartBackgroundImageLoads();

  nsFrameConstructorSaveState docElementContainingBlockAbsoluteSaveState;
  if (mHasRootAbsPosContainingBlock) {
    // Push the absolute containing block now so we can absolutely position
    // the root element
    mDocElementContainingBlock->AddStateBits(NS_FRAME_CAN_HAVE_ABSPOS_CHILDREN);
    state.PushAbsoluteContainingBlock(mDocElementContainingBlock,
                                      mDocElementContainingBlock,
                                      docElementContainingBlockAbsoluteSaveState);
  }

  // The rules from CSS 2.1, section 9.2.4, have already been applied
  // by the style system, so we can assume that display->mDisplay is
  // either NONE, BLOCK, or TABLE.

  // contentFrame is the primary frame for the root element. newFrame
  // is the frame that will be the child of the initial containing block.
  // These are usually the same frame but they can be different, in
  // particular if the root frame is positioned, in which case
  // contentFrame is the out-of-flow frame and newFrame is the
  // placeholder.
  nsContainerFrame* contentFrame;
  nsIFrame* newFrame;
  bool processChildren = false;

  nsFrameConstructorSaveState absoluteSaveState;

  // Check whether we need to build a XUL box or SVG root frame
#ifdef MOZ_XUL
  if (aDocElement->IsXULElement()) {
    contentFrame = NS_NewDocElementBoxFrame(mPresShell, styleContext);
    InitAndRestoreFrame(state, aDocElement, mDocElementContainingBlock,
                        contentFrame);
    newFrame = contentFrame;
    processChildren = true;
  }
  else
#endif
  if (aDocElement->IsSVGElement()) {
    if (!aDocElement->IsSVGElement(nsGkAtoms::svg)) {
      return nullptr;
    }
    // We're going to call the right function ourselves, so no need to give a
    // function to this FrameConstructionData.

    // XXXbz on the other hand, if we converted this whole function to
    // FrameConstructionData/Item, then we'd need the right function
    // here... but would probably be able to get away with less code in this
    // function in general.
    // Use a null PendingBinding, since our binding is not in fact pending.
    static const FrameConstructionData rootSVGData = FCDATA_DECL(0, nullptr);
    already_AddRefed<nsStyleContext> extraRef =
      RefPtr<nsStyleContext>(styleContext).forget();
    FrameConstructionItem item(&rootSVGData, aDocElement,
                               aDocElement->NodeInfo()->NameAtom(),
                               kNameSpaceID_SVG, nullptr, extraRef, true,
                               nullptr);

    nsFrameItems frameItems;
    contentFrame = static_cast<nsContainerFrame*>(
      ConstructOuterSVG(state, item, mDocElementContainingBlock,
                        styleContext->StyleDisplay(),
                        frameItems));
    newFrame = frameItems.FirstChild();
    NS_ASSERTION(frameItems.OnlyChild(), "multiple root element frames");
  } else if (display->mDisplay == StyleDisplay::Flex ||
             display->mDisplay == StyleDisplay::WebkitBox) {
    contentFrame = NS_NewFlexContainerFrame(mPresShell, styleContext);
    InitAndRestoreFrame(state, aDocElement, mDocElementContainingBlock,
                        contentFrame);
    newFrame = contentFrame;
    processChildren = true;

    newFrame->AddStateBits(NS_FRAME_CAN_HAVE_ABSPOS_CHILDREN);
    if (display->IsAbsPosContainingBlock(newFrame)) {
      state.PushAbsoluteContainingBlock(contentFrame, newFrame,
                                        absoluteSaveState);
    }

  } else if (display->mDisplay == StyleDisplay::Grid) {
    contentFrame = NS_NewGridContainerFrame(mPresShell, styleContext);
    InitAndRestoreFrame(state, aDocElement, mDocElementContainingBlock,
                        contentFrame);
    newFrame = contentFrame;
    processChildren = true;

    newFrame->AddStateBits(NS_FRAME_CAN_HAVE_ABSPOS_CHILDREN);
    if (display->IsAbsPosContainingBlock(newFrame)) {
      state.PushAbsoluteContainingBlock(contentFrame, newFrame,
                                        absoluteSaveState);
    }
  } else if (display->mDisplay == StyleDisplay::Table) {
    // We're going to call the right function ourselves, so no need to give a
    // function to this FrameConstructionData.

    // XXXbz on the other hand, if we converted this whole function to
    // FrameConstructionData/Item, then we'd need the right function
    // here... but would probably be able to get away with less code in this
    // function in general.
    // Use a null PendingBinding, since our binding is not in fact pending.
    static const FrameConstructionData rootTableData = FCDATA_DECL(0, nullptr);
    already_AddRefed<nsStyleContext> extraRef =
      RefPtr<nsStyleContext>(styleContext).forget();
    FrameConstructionItem item(&rootTableData, aDocElement,
                               aDocElement->NodeInfo()->NameAtom(),
                               kNameSpaceID_None, nullptr, extraRef, true,
                               nullptr);

    nsFrameItems frameItems;
    // if the document is a table then just populate it.
    contentFrame = static_cast<nsContainerFrame*>(
      ConstructTable(state, item, mDocElementContainingBlock,
                     styleContext->StyleDisplay(),
                     frameItems));
    newFrame = frameItems.FirstChild();
    NS_ASSERTION(frameItems.OnlyChild(), "multiple root element frames");
  } else {
    MOZ_ASSERT(display->mDisplay == StyleDisplay::Block,
               "Unhandled display type for root element");
    contentFrame = NS_NewBlockFormattingContext(mPresShell, styleContext);
    nsFrameItems frameItems;
    // Use a null PendingBinding, since our binding is not in fact pending.
    ConstructBlock(state, aDocElement,
                   state.GetGeometricParent(display,
                                            mDocElementContainingBlock),
                   mDocElementContainingBlock, styleContext,
                   &contentFrame, frameItems,
                   display->IsAbsPosContainingBlock(contentFrame) ? contentFrame : nullptr,
                   nullptr);
    newFrame = frameItems.FirstChild();
    NS_ASSERTION(frameItems.OnlyChild(), "multiple root element frames");
  }

  MOZ_ASSERT(newFrame);
  MOZ_ASSERT(contentFrame);

  NS_ASSERTION(processChildren ? !mRootElementFrame :
                 mRootElementFrame == contentFrame,
               "unexpected mRootElementFrame");
  mRootElementFrame = contentFrame;

  // Figure out which frame has the main style for the document element,
  // assigning it to mRootElementStyleFrame.
  // Backgrounds should be propagated from that frame to the viewport.
  contentFrame->GetParentStyleContext(&mRootElementStyleFrame);
  bool isChild = mRootElementStyleFrame &&
                 mRootElementStyleFrame->GetParent() == contentFrame;
  if (!isChild) {
    mRootElementStyleFrame = mRootElementFrame;
  }

  if (processChildren) {
    // Still need to process the child content
    nsFrameItems childItems;

    NS_ASSERTION(!nsLayoutUtils::GetAsBlock(contentFrame) &&
                 !contentFrame->IsFrameOfType(nsIFrame::eSVG),
                 "Only XUL frames should reach here");
    // Use a null PendingBinding, since our binding is not in fact pending.
    ProcessChildren(state, aDocElement, styleContext, contentFrame, true,
                    childItems, false, nullptr);

    // Set the initial child lists
    contentFrame->SetInitialChildList(kPrincipalList, childItems);
  }

  // set the primary frame
  aDocElement->SetPrimaryFrame(contentFrame);

  SetInitialSingleChild(mDocElementContainingBlock, newFrame);

  // Create frames for anonymous contents if there is a canvas frame.
  if (mDocElementContainingBlock->GetType() == nsGkAtoms::canvasFrame) {
    ConstructAnonymousContentForCanvas(state, mDocElementContainingBlock,
                                       aDocElement);
  }

  return newFrame;
}


nsIFrame*
nsCSSFrameConstructor::ConstructRootFrame()
{
  AUTO_LAYOUT_PHASE_ENTRY_POINT(mPresShell->GetPresContext(), FrameC);

  StyleSetHandle styleSet = mPresShell->StyleSet();

  // Set up our style rule observer.
  // XXXbz wouldn't this make more sense as part of presshell init?
  if (styleSet->IsGecko()) {
    // XXXheycam We don't support XBL bindings providing style to
    // ServoStyleSets yet.
    styleSet->AsGecko()->SetBindingManager(mDocument->BindingManager());
  } else {
    NS_WARNING("stylo: cannot get ServoStyleSheets from XBL bindings yet. See bug 1290276.");
  }

  // --------- BUILD VIEWPORT -----------
  RefPtr<nsStyleContext> viewportPseudoStyle =
    styleSet->ResolveAnonymousBoxStyle(nsCSSAnonBoxes::viewport, nullptr);
  ViewportFrame* viewportFrame =
    NS_NewViewportFrame(mPresShell, viewportPseudoStyle);

  // XXXbz do we _have_ to pass a null content pointer to that frame?
  // Would it really kill us to pass in the root element or something?
  // What would that break?
  viewportFrame->Init(nullptr, nullptr, nullptr);

  // Bind the viewport frame to the root view
  nsView* rootView = mPresShell->GetViewManager()->GetRootView();
  viewportFrame->SetView(rootView);

  nsContainerFrame::SyncFrameViewProperties(mPresShell->GetPresContext(), viewportFrame,
                                            viewportPseudoStyle, rootView);
  nsContainerFrame::SyncWindowProperties(mPresShell->GetPresContext(), viewportFrame,
                                         rootView, nullptr, nsContainerFrame::SET_ASYNC);

  // Make it an absolute container for fixed-pos elements
  viewportFrame->AddStateBits(NS_FRAME_CAN_HAVE_ABSPOS_CHILDREN);
  viewportFrame->MarkAsAbsoluteContainingBlock();

  return viewportFrame;
}

void
nsCSSFrameConstructor::SetUpDocElementContainingBlock(nsIContent* aDocElement)
{
  NS_PRECONDITION(aDocElement, "No element?");
  NS_PRECONDITION(!aDocElement->GetParent(), "Not root content?");
  NS_PRECONDITION(aDocElement->GetUncomposedDoc(), "Not in a document?");
  NS_PRECONDITION(aDocElement->GetUncomposedDoc()->GetRootElement() ==
                  aDocElement, "Not the root of the document?");

  /*
    how the root frame hierarchy should look

  Galley presentation, non-XUL, with scrolling:

      ViewportFrame [fixed-cb]
        nsHTMLScrollFrame
          nsCanvasFrame [abs-cb]
            root element frame (nsBlockFrame, nsSVGOuterSVGFrame,
                                nsTableWrapperFrame, nsPlaceholderFrame)

  Galley presentation, XUL

      ViewportFrame [fixed-cb]
        nsRootBoxFrame
          root element frame (nsDocElementBoxFrame)

  Print presentation, non-XUL

      ViewportFrame
        nsSimplePageSequenceFrame
          nsPageFrame
            nsPageContentFrame [fixed-cb]
              nsCanvasFrame [abs-cb]
                root element frame (nsBlockFrame, nsSVGOuterSVGFrame,
                                    nsTableWrapperFrame, nsPlaceholderFrame)

  Print-preview presentation, non-XUL

      ViewportFrame
        nsHTMLScrollFrame
          nsSimplePageSequenceFrame
            nsPageFrame
              nsPageContentFrame [fixed-cb]
                nsCanvasFrame [abs-cb]
                  root element frame (nsBlockFrame, nsSVGOuterSVGFrame,
                                      nsTableWrapperFrame, nsPlaceholderFrame)

  Print/print preview of XUL is not supported.
  [fixed-cb]: the default containing block for fixed-pos content
  [abs-cb]: the default containing block for abs-pos content

  Meaning of nsCSSFrameConstructor fields:
    mRootElementFrame is "root element frame".  This is the primary frame for
      the root element.
    mDocElementContainingBlock is the parent of mRootElementFrame
      (i.e. nsCanvasFrame or nsRootBoxFrame)
    mGfxScrollFrame is the nsHTMLScrollFrame mentioned above, or null if there isn't one
    mPageSequenceFrame is the nsSimplePageSequenceFrame, or null if there isn't one
  */

  // --------- CREATE ROOT FRAME -------


  // Create the root frame. The document element's frame is a child of the
  // root frame.
  //
  // The root frame serves two purposes:
  // - reserves space for any margins needed for the document element's frame
  // - renders the document element's background. This ensures the background covers
  //   the entire canvas as specified by the CSS2 spec

  nsPresContext* presContext = mPresShell->GetPresContext();
  bool isPaginated = presContext->IsRootPaginatedDocument();
  nsContainerFrame* viewportFrame = static_cast<nsContainerFrame*>(GetRootFrame());
  nsStyleContext* viewportPseudoStyle = viewportFrame->StyleContext();

  nsContainerFrame* rootFrame = nullptr;
  nsIAtom* rootPseudo;

  if (!isPaginated) {
#ifdef MOZ_XUL
    if (aDocElement->IsXULElement())
    {
      // pass a temporary stylecontext, the correct one will be set later
      rootFrame = NS_NewRootBoxFrame(mPresShell, viewportPseudoStyle);
    } else
#endif
    {
      // pass a temporary stylecontext, the correct one will be set later
      rootFrame = NS_NewCanvasFrame(mPresShell, viewportPseudoStyle);
      mHasRootAbsPosContainingBlock = true;
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

  bool isHTML = aDocElement->IsHTMLElement();
  bool isXUL = false;

  if (!isHTML) {
    isXUL = aDocElement->IsXULElement();
  }

  // Never create scrollbars for XUL documents
  bool isScrollable = isPaginated ? presContext->HasPaginatedScrolling() : !isXUL;

  // We no longer need to do overflow propagation here. It's taken care of
  // when we construct frames for the element whose overflow might be
  // propagated
  NS_ASSERTION(!isScrollable || !isXUL,
               "XUL documents should never be scrollable - see above");

  nsContainerFrame* newFrame = rootFrame;
  RefPtr<nsStyleContext> rootPseudoStyle;
  // we must create a state because if the scrollbars are GFX it needs the
  // state to build the scrollbar frames.
  nsFrameConstructorState state(mPresShell, nullptr, nullptr, nullptr);

  // Start off with the viewport as parent; we'll adjust it as needed.
  nsContainerFrame* parentFrame = viewportFrame;

  StyleSetHandle styleSet = mPresShell->StyleSet();
  // If paginated, make sure we don't put scrollbars in
  if (!isScrollable) {
    rootPseudoStyle = styleSet->ResolveAnonymousBoxStyle(rootPseudo,
                                                         viewportPseudoStyle);
  } else {
      if (rootPseudo == nsCSSAnonBoxes::canvas) {
        rootPseudo = nsCSSAnonBoxes::scrolledCanvas;
      } else {
        NS_ASSERTION(rootPseudo == nsCSSAnonBoxes::pageSequence,
                     "Unknown root pseudo");
        rootPseudo = nsCSSAnonBoxes::scrolledPageSequence;
      }

      // Build the frame. We give it the content we are wrapping which is the
      // document element, the root frame, the parent view port frame, and we
      // should get back the new frame and the scrollable view if one was
      // created.

      // resolve a context for the scrollframe
      RefPtr<nsStyleContext>  styleContext;
      styleContext = styleSet->ResolveAnonymousBoxStyle(nsCSSAnonBoxes::viewportScroll,
                                                        viewportPseudoStyle);

      // Note that the viewport scrollframe is always built with
      // overflow:auto style. This forces the scroll frame to create
      // anonymous content for both scrollbars. This is necessary even
      // if the HTML or BODY elements are overriding the viewport
      // scroll style to 'hidden' --- dynamic style changes might put
      // scrollbars back on the viewport and we don't want to have to
      // reframe the viewport to create the scrollbar content.
      newFrame = nullptr;
      rootPseudoStyle = BeginBuildingScrollFrame( state,
                                                  aDocElement,
                                                  styleContext,
                                                  viewportFrame,
                                                  rootPseudo,
                                                  true,
                                                  newFrame);
      parentFrame = newFrame;
      mGfxScrollFrame = newFrame;
  }

  rootFrame->SetStyleContextWithoutNotification(rootPseudoStyle);
  rootFrame->Init(aDocElement, parentFrame, nullptr);

  if (isScrollable) {
    FinishBuildingScrollFrame(parentFrame, rootFrame);
  }

  if (isPaginated) {
    // Create the first page
    // Set the initial child lists
    nsContainerFrame* canvasFrame;
    nsContainerFrame* pageFrame =
      ConstructPageFrame(mPresShell, rootFrame, nullptr, canvasFrame);
    SetInitialSingleChild(rootFrame, pageFrame);

    // The eventual parent of the document element frame.
    // XXX should this be set for every new page (in ConstructPageFrame)?
    mDocElementContainingBlock = canvasFrame;
    mHasRootAbsPosContainingBlock = true;
  }

  if (viewportFrame->GetStateBits() & NS_FRAME_FIRST_REFLOW) {
    SetInitialSingleChild(viewportFrame, newFrame);
  } else {
    nsFrameList newFrameList(newFrame, newFrame);
    viewportFrame->AppendFrames(kPrincipalList, newFrameList);
  }
}

void
nsCSSFrameConstructor::ConstructAnonymousContentForCanvas(nsFrameConstructorState& aState,
                                                          nsIFrame* aFrame,
                                                          nsIContent* aDocElement)
{
  NS_ASSERTION(aFrame->GetType() == nsGkAtoms::canvasFrame, "aFrame should be canvas frame!");

  AutoTArray<nsIAnonymousContentCreator::ContentInfo, 4> anonymousItems;
  GetAnonymousContent(aDocElement, aFrame, anonymousItems);
  if (anonymousItems.IsEmpty()) {
    return;
  }

  FrameConstructionItemList itemsToConstruct;
  nsContainerFrame* frameAsContainer = do_QueryFrame(aFrame);
  AddFCItemsForAnonymousContent(aState, frameAsContainer, anonymousItems, itemsToConstruct);

  nsFrameItems frameItems;
  ConstructFramesFromItemList(aState, itemsToConstruct, frameAsContainer, frameItems);
  frameAsContainer->AppendFrames(kPrincipalList, frameItems);
}

nsContainerFrame*
nsCSSFrameConstructor::ConstructPageFrame(nsIPresShell*  aPresShell,
                                          nsContainerFrame* aParentFrame,
                                          nsIFrame*      aPrevPageFrame,
                                          nsContainerFrame*& aCanvasFrame)
{
  nsStyleContext* parentStyleContext = aParentFrame->StyleContext();
  StyleSetHandle styleSet = aPresShell->StyleSet();

  RefPtr<nsStyleContext> pagePseudoStyle;
  pagePseudoStyle = styleSet->ResolveAnonymousBoxStyle(nsCSSAnonBoxes::page,
                                                       parentStyleContext);

  nsContainerFrame* pageFrame = NS_NewPageFrame(aPresShell, pagePseudoStyle);

  // Initialize the page frame and force it to have a view. This makes printing of
  // the pages easier and faster.
  pageFrame->Init(nullptr, aParentFrame, aPrevPageFrame);

  RefPtr<nsStyleContext> pageContentPseudoStyle;
  pageContentPseudoStyle =
    styleSet->ResolveAnonymousBoxStyle(nsCSSAnonBoxes::pageContent,
                                       pagePseudoStyle);

  nsContainerFrame* pageContentFrame =
    NS_NewPageContentFrame(aPresShell, pageContentPseudoStyle);

  // Initialize the page content frame and force it to have a view. Also make it the
  // containing block for fixed elements which are repeated on every page.
  nsIFrame* prevPageContentFrame = nullptr;
  if (aPrevPageFrame) {
    prevPageContentFrame = aPrevPageFrame->PrincipalChildList().FirstChild();
    NS_ASSERTION(prevPageContentFrame, "missing page content frame");
  }
  pageContentFrame->Init(nullptr, pageFrame, prevPageContentFrame);
  SetInitialSingleChild(pageFrame, pageContentFrame);
  // Make it an absolute container for fixed-pos elements
  pageContentFrame->AddStateBits(NS_FRAME_CAN_HAVE_ABSPOS_CHILDREN);
  pageContentFrame->MarkAsAbsoluteContainingBlock();

  RefPtr<nsStyleContext> canvasPseudoStyle;
  canvasPseudoStyle = styleSet->ResolveAnonymousBoxStyle(nsCSSAnonBoxes::canvas,
                                                         pageContentPseudoStyle);

  aCanvasFrame = NS_NewCanvasFrame(aPresShell, canvasPseudoStyle);

  nsIFrame* prevCanvasFrame = nullptr;
  if (prevPageContentFrame) {
    prevCanvasFrame = prevPageContentFrame->PrincipalChildList().FirstChild();
    NS_ASSERTION(prevCanvasFrame, "missing canvas frame");
  }
  aCanvasFrame->Init(nullptr, pageContentFrame, prevCanvasFrame);
  SetInitialSingleChild(pageContentFrame, aCanvasFrame);
  return pageFrame;
}

/* static */
nsIFrame*
nsCSSFrameConstructor::CreatePlaceholderFrameFor(nsIPresShell*     aPresShell,
                                                 nsIContent*       aContent,
                                                 nsIFrame*         aFrame,
                                                 nsStyleContext*   aParentStyle,
                                                 nsContainerFrame* aParentFrame,
                                                 nsIFrame*         aPrevInFlow,
                                                 nsFrameState      aTypeBit)
{
  RefPtr<nsStyleContext> placeholderStyle = aPresShell->StyleSet()->
    ResolveStyleForOtherNonElement(aParentStyle);

  // The placeholder frame gets a pseudo style context
  nsPlaceholderFrame* placeholderFrame =
    (nsPlaceholderFrame*)NS_NewPlaceholderFrame(aPresShell, placeholderStyle,
                                                aTypeBit);

  placeholderFrame->Init(aContent, aParentFrame, aPrevInFlow);

  // The placeholder frame has a pointer back to the out-of-flow frame
  placeholderFrame->SetOutOfFlowFrame(aFrame);

  aFrame->AddStateBits(NS_FRAME_OUT_OF_FLOW);

  // Add mapping from absolutely positioned frame to its placeholder frame
  aPresShell->FrameManager()->RegisterPlaceholderFrame(placeholderFrame);

  return placeholderFrame;
}

// Clears any lazy bits set in the range [aStartContent, aEndContent).  If
// aEndContent is null, that means to clear bits in all siblings starting with
// aStartContent.  aStartContent must not be null unless aEndContent is also
// null.  We do this so that when new children are inserted under elements whose
// frame is a leaf the new children don't cause us to try to construct frames
// for the existing children again.
static inline void
ClearLazyBits(nsIContent* aStartContent, nsIContent* aEndContent)
{
  NS_PRECONDITION(aStartContent || !aEndContent,
                  "Must have start child if we have an end child");
  for (nsIContent* cur = aStartContent; cur != aEndContent;
       cur = cur->GetNextSibling()) {
    cur->UnsetFlags(NODE_DESCENDANTS_NEED_FRAMES | NODE_NEEDS_FRAME);
  }
}

nsIFrame*
nsCSSFrameConstructor::ConstructSelectFrame(nsFrameConstructorState& aState,
                                            FrameConstructionItem&   aItem,
                                            nsContainerFrame*        aParentFrame,
                                            const nsStyleDisplay*    aStyleDisplay,
                                            nsFrameItems&            aFrameItems)
{
  nsIContent* const content = aItem.mContent;
  nsStyleContext* const styleContext = aItem.mStyleContext;

  // Construct a frame-based listbox or combobox
  dom::HTMLSelectElement* sel = dom::HTMLSelectElement::FromContent(content);
  MOZ_ASSERT(sel);
  if (sel->IsCombobox()) {
    // Construct a frame-based combo box.
    // The frame-based combo box is built out of three parts. A display area, a button and
    // a dropdown list. The display area and button are created through anonymous content.
    // The drop-down list's frame is created explicitly. The combobox frame shares its content
    // with the drop-down list.
    nsFrameState flags = NS_BLOCK_FLOAT_MGR;
    nsContainerFrame* comboboxFrame =
      NS_NewComboboxControlFrame(mPresShell, styleContext, flags);

    // Save the history state so we don't restore during construction
    // since the complete tree is required before we restore.
    nsILayoutHistoryState *historyState = aState.mFrameState;
    aState.mFrameState = nullptr;
    // Initialize the combobox frame
    InitAndRestoreFrame(aState, content,
                        aState.GetGeometricParent(aStyleDisplay, aParentFrame),
                        comboboxFrame);

    aState.AddChild(comboboxFrame, aFrameItems, content, styleContext,
                    aParentFrame);

    nsIComboboxControlFrame* comboBox = do_QueryFrame(comboboxFrame);
    NS_ASSERTION(comboBox, "NS_NewComboboxControlFrame returned frame that "
                 "doesn't implement nsIComboboxControlFrame");

    // Resolve pseudo element style for the dropdown list
    RefPtr<nsStyleContext> listStyle;
    listStyle = mPresShell->StyleSet()->
      ResolveAnonymousBoxStyle(nsCSSAnonBoxes::dropDownList, styleContext);

    // Create a listbox
    nsContainerFrame* listFrame = NS_NewListControlFrame(mPresShell, listStyle);

    // Notify the listbox that it is being used as a dropdown list.
    nsIListControlFrame * listControlFrame = do_QueryFrame(listFrame);
    if (listControlFrame) {
      listControlFrame->SetComboboxFrame(comboboxFrame);
    }
    // Notify combobox that it should use the listbox as it's popup
    comboBox->SetDropDown(listFrame);

    NS_ASSERTION(!listFrame->IsAbsPosContainingBlock(),
                 "Ended up with positioned dropdown list somehow.");
    NS_ASSERTION(!listFrame->IsFloating(),
                 "Ended up with floating dropdown list somehow.");

    // Initialize the scroll frame positioned. Note that it is NOT
    // initialized as absolutely positioned.
    nsContainerFrame* scrolledFrame =
      NS_NewSelectsAreaFrame(mPresShell, styleContext, flags);

    InitializeSelectFrame(aState, listFrame, scrolledFrame, content,
                          comboboxFrame, listStyle, true,
                          aItem.mPendingBinding, aFrameItems);

    NS_ASSERTION(listFrame->GetView(), "ListFrame's view is nullptr");

    // Create display and button frames from the combobox's anonymous content.
    // The anonymous content is appended to existing anonymous content for this
    // element (the scrollbars).

    nsFrameItems childItems;
    CreateAnonymousFrames(aState, content, comboboxFrame,
                          aItem.mPendingBinding, childItems);

    comboboxFrame->SetInitialChildList(kPrincipalList, childItems);

    // Initialize the additional popup child list which contains the
    // dropdown list frame.
    nsFrameItems popupItems;
    popupItems.AddChild(listFrame);
    comboboxFrame->SetInitialChildList(nsIFrame::kSelectPopupList,
                                       popupItems);

    aState.mFrameState = historyState;
    if (aState.mFrameState) {
      // Restore frame state for the entire subtree of |comboboxFrame|.
      RestoreFrameState(comboboxFrame, aState.mFrameState);
    }
    return comboboxFrame;
  }

  // Listbox, not combobox
  nsContainerFrame* listFrame = NS_NewListControlFrame(mPresShell, styleContext);

  nsContainerFrame* scrolledFrame = NS_NewSelectsAreaFrame(
      mPresShell, styleContext, NS_BLOCK_FLOAT_MGR);

  // ******* this code stolen from Initialze ScrollFrame ********
  // please adjust this code to use BuildScrollFrame.

  InitializeSelectFrame(aState, listFrame, scrolledFrame, content,
                        aParentFrame, styleContext, false,
                        aItem.mPendingBinding, aFrameItems);

  return listFrame;
}

/**
 * Used to be InitializeScrollFrame but now it's only used for the select tag
 * But the select tag should really be fixed to use GFX scrollbars that can
 * be create with BuildScrollFrame.
 */
nsresult
nsCSSFrameConstructor::InitializeSelectFrame(nsFrameConstructorState& aState,
                                             nsContainerFrame*        scrollFrame,
                                             nsContainerFrame*        scrolledFrame,
                                             nsIContent*              aContent,
                                             nsContainerFrame*        aParentFrame,
                                             nsStyleContext*          aStyleContext,
                                             bool                     aBuildCombobox,
                                             PendingBinding*          aPendingBinding,
                                             nsFrameItems&            aFrameItems)
{
  // Initialize it
  nsContainerFrame* geometricParent =
    aState.GetGeometricParent(aStyleContext->StyleDisplay(), aParentFrame);

  // We don't call InitAndRestoreFrame for scrollFrame because we can only
  // restore the frame state after its parts have been created (in particular,
  // the scrollable view). So we have to split Init and Restore.

  scrollFrame->Init(aContent, geometricParent, nullptr);

  if (!aBuildCombobox) {
    aState.AddChild(scrollFrame, aFrameItems, aContent,
                    aStyleContext, aParentFrame);
  }

  if (aBuildCombobox) {
    nsContainerFrame::CreateViewForFrame(scrollFrame, true);
  }

  BuildScrollFrame(aState, aContent, aStyleContext, scrolledFrame,
                   geometricParent, scrollFrame);

  if (aState.mFrameState) {
    // Restore frame state for the scroll frame
    RestoreFrameStateFor(scrollFrame, aState.mFrameState);
  }

  // Process children
  nsFrameItems                childItems;

  ProcessChildren(aState, aContent, aStyleContext, scrolledFrame, false,
                  childItems, false, aPendingBinding);

  // Set the scrolled frame's initial child lists
  scrolledFrame->SetInitialChildList(kPrincipalList, childItems);
  return NS_OK;
}

nsIFrame*
nsCSSFrameConstructor::ConstructFieldSetFrame(nsFrameConstructorState& aState,
                                              FrameConstructionItem&   aItem,
                                              nsContainerFrame*        aParentFrame,
                                              const nsStyleDisplay*    aStyleDisplay,
                                              nsFrameItems&            aFrameItems)
{
  nsIContent* const content = aItem.mContent;
  nsStyleContext* const styleContext = aItem.mStyleContext;

  nsContainerFrame* fieldsetFrame = NS_NewFieldSetFrame(mPresShell, styleContext);

  // Initialize it
  InitAndRestoreFrame(aState, content,
                      aState.GetGeometricParent(aStyleDisplay, aParentFrame),
                      fieldsetFrame);

  // Resolve style and initialize the frame
  RefPtr<nsStyleContext> fieldsetContentStyle;
  fieldsetContentStyle = mPresShell->StyleSet()->
    ResolveAnonymousBoxStyle(nsCSSAnonBoxes::fieldsetContent, styleContext);

  const nsStyleDisplay* fieldsetContentDisplay = fieldsetContentStyle->StyleDisplay();
  bool isScrollable = fieldsetContentDisplay->IsScrollableOverflow();
  nsContainerFrame* scrollFrame = nullptr;
  if (isScrollable) {
    fieldsetContentStyle =
      BeginBuildingScrollFrame(aState, content, fieldsetContentStyle,
                               fieldsetFrame, nsCSSAnonBoxes::scrolledContent,
                               false, scrollFrame);
  }

  nsContainerFrame* absPosContainer = nullptr;
  if (fieldsetFrame->IsAbsPosContainingBlock()) {
    absPosContainer = fieldsetFrame;
  }

  // Create the inner ::-moz-fieldset-content frame.
  nsContainerFrame* contentFrameTop;
  nsContainerFrame* contentFrame;
  auto parent = scrollFrame ? scrollFrame : fieldsetFrame;
  switch (fieldsetContentDisplay->mDisplay) {
    case StyleDisplay::Flex:
      contentFrame = NS_NewFlexContainerFrame(mPresShell, fieldsetContentStyle);
      InitAndRestoreFrame(aState, content, parent, contentFrame);
      contentFrameTop = contentFrame;
      break;
    case StyleDisplay::Grid:
      contentFrame = NS_NewGridContainerFrame(mPresShell, fieldsetContentStyle);
      InitAndRestoreFrame(aState, content, parent, contentFrame);
      contentFrameTop = contentFrame;
      break;
    default: {
      MOZ_ASSERT(fieldsetContentDisplay->mDisplay == StyleDisplay::Block,
                 "bug in nsRuleNode::ComputeDisplayData?");

      nsContainerFrame* columnSetFrame = nullptr;
      RefPtr<nsStyleContext> innerSC = fieldsetContentStyle;
      const nsStyleColumn* columns = fieldsetContentStyle->StyleColumn();
      if (columns->mColumnCount != NS_STYLE_COLUMN_COUNT_AUTO ||
          columns->mColumnWidth.GetUnit() != eStyleUnit_Auto) {
        columnSetFrame =
          NS_NewColumnSetFrame(mPresShell, fieldsetContentStyle, nsFrameState(0));
        InitAndRestoreFrame(aState, content, parent, columnSetFrame);
        innerSC = mPresShell->StyleSet()->ResolveAnonymousBoxStyle(
                    nsCSSAnonBoxes::columnContent, fieldsetContentStyle);
        if (absPosContainer) {
          absPosContainer = columnSetFrame;
        }
      }
      contentFrame = NS_NewBlockFormattingContext(mPresShell, innerSC);
      if (columnSetFrame) {
        InitAndRestoreFrame(aState, content, columnSetFrame, contentFrame);
        SetInitialSingleChild(columnSetFrame, contentFrame);
        contentFrameTop = columnSetFrame;
      } else {
        InitAndRestoreFrame(aState, content, parent, contentFrame);
        contentFrameTop = contentFrame;
      }
      break;
    }
  }

  aState.AddChild(fieldsetFrame, aFrameItems, content, styleContext, aParentFrame);

  // Process children
  nsFrameConstructorSaveState absoluteSaveState;
  nsFrameItems                childItems;

  contentFrame->AddStateBits(NS_FRAME_CAN_HAVE_ABSPOS_CHILDREN);
  if (absPosContainer) {
    aState.PushAbsoluteContainingBlock(contentFrame, absPosContainer, absoluteSaveState);
  }

  ProcessChildren(aState, content, styleContext, contentFrame, true,
                  childItems, true, aItem.mPendingBinding);

  nsFrameItems fieldsetKids;
  fieldsetKids.AddChild(scrollFrame ? scrollFrame : contentFrameTop);

  for (nsFrameList::Enumerator e(childItems); !e.AtEnd(); e.Next()) {
    nsIFrame* child = e.get();
    nsContainerFrame* cif = child->GetContentInsertionFrame();
    if (cif && cif->GetType() == nsGkAtoms::legendFrame) {
      // We want the legend to be the first frame in the fieldset child list.
      // That way the EventStateManager will do the right thing when tabbing
      // from a selection point within the legend (bug 236071), which is
      // used for implementing legend access keys (bug 81481).
      // GetAdjustedParentFrame() below depends on this frame order.
      childItems.RemoveFrame(child);
      // Make sure to reparent the legend so it has the fieldset as the parent.
      fieldsetKids.InsertFrame(fieldsetFrame, nullptr, child);
      if (scrollFrame) {
        StickyScrollContainer::NotifyReparentedFrameAcrossScrollFrameBoundary(
            child, contentFrame);
      }
      break;
    }
  }

  if (isScrollable) {
    FinishBuildingScrollFrame(scrollFrame, contentFrameTop);
  }

  // Set the inner frame's initial child lists
  contentFrame->SetInitialChildList(kPrincipalList, childItems);

  // Set the outer frame's initial child list
  fieldsetFrame->SetInitialChildList(kPrincipalList, fieldsetKids);

  fieldsetFrame->AddStateBits(NS_FRAME_MAY_HAVE_GENERATED_CONTENT);

  // Our new frame returned is the outer frame, which is the fieldset frame.
  return fieldsetFrame;
}

nsIFrame*
nsCSSFrameConstructor::ConstructDetailsFrame(nsFrameConstructorState& aState,
                                             FrameConstructionItem& aItem,
                                             nsContainerFrame* aParentFrame,
                                             const nsStyleDisplay* aStyleDisplay,
                                             nsFrameItems& aFrameItems)
{
  if (!aStyleDisplay->IsScrollableOverflow()) {
    return ConstructNonScrollableBlockWithConstructor(aState, aItem, aParentFrame,
                                                      aStyleDisplay, aFrameItems,
                                                      NS_NewDetailsFrame);
  }

  // Build a scroll frame to wrap details frame if necessary.
  return ConstructScrollableBlockWithConstructor(aState, aItem, aParentFrame,
                                                 aStyleDisplay, aFrameItems,
                                                 NS_NewDetailsFrame);
}

static nsIFrame*
FindAncestorWithGeneratedContentPseudo(nsIFrame* aFrame)
{
  for (nsIFrame* f = aFrame->GetParent(); f; f = f->GetParent()) {
    NS_ASSERTION(f->IsGeneratedContentFrame(),
                 "should not have exited generated content");
    nsIAtom* pseudo = f->StyleContext()->GetPseudo();
    if (pseudo == nsCSSPseudoElements::before ||
        pseudo == nsCSSPseudoElements::after)
      return f;
  }
  return nullptr;
}

#define SIMPLE_FCDATA(_func) FCDATA_DECL(0, _func)
#define FULL_CTOR_FCDATA(_flags, _func)                             \
  { _flags | FCDATA_FUNC_IS_FULL_CTOR, { nullptr }, _func, nullptr }

/* static */
const nsCSSFrameConstructor::FrameConstructionData*
nsCSSFrameConstructor::FindTextData(nsIFrame* aParentFrame)
{
  if (aParentFrame && IsFrameForSVG(aParentFrame)) {
    nsIFrame *ancestorFrame =
      nsSVGUtils::GetFirstNonAAncestorFrame(aParentFrame);
    if (ancestorFrame) {
      static const FrameConstructionData sSVGTextData =
        FCDATA_DECL(FCDATA_IS_LINE_PARTICIPANT | FCDATA_IS_SVG_TEXT,
                    NS_NewTextFrame);
      if (ancestorFrame->IsSVGText()) {
        return &sSVGTextData;
      }
    }
    return nullptr;
  }

  static const FrameConstructionData sTextData =
    FCDATA_DECL(FCDATA_IS_LINE_PARTICIPANT, NS_NewTextFrame);
  return &sTextData;
}

void
nsCSSFrameConstructor::ConstructTextFrame(const FrameConstructionData* aData,
                                          nsFrameConstructorState& aState,
                                          nsIContent*              aContent,
                                          nsContainerFrame*        aParentFrame,
                                          nsStyleContext*          aStyleContext,
                                          nsFrameItems&            aFrameItems)
{
  NS_PRECONDITION(aData, "Must have frame construction data");

  nsIFrame* newFrame = (*aData->mFunc.mCreationFunc)(mPresShell, aStyleContext);

  InitAndRestoreFrame(aState, aContent, aParentFrame, newFrame);

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

  if (!aState.mCreatingExtraFrames)
    aContent->SetPrimaryFrame(newFrame);
}

/* static */
const nsCSSFrameConstructor::FrameConstructionData*
nsCSSFrameConstructor::FindDataByInt(int32_t aInt,
                                     Element* aElement,
                                     nsStyleContext* aStyleContext,
                                     const FrameConstructionDataByInt* aDataPtr,
                                     uint32_t aDataLength)
{
  for (const FrameConstructionDataByInt *curData = aDataPtr,
         *endData = aDataPtr + aDataLength;
       curData != endData;
       ++curData) {
    if (curData->mInt == aInt) {
      const FrameConstructionData* data = &curData->mData;
      if (data->mBits & FCDATA_FUNC_IS_DATA_GETTER) {
        return data->mFunc.mDataGetter(aElement, aStyleContext);
      }

      return data;
    }
  }

  return nullptr;
}

/* static */
const nsCSSFrameConstructor::FrameConstructionData*
nsCSSFrameConstructor::FindDataByTag(nsIAtom* aTag,
                                     Element* aElement,
                                     nsStyleContext* aStyleContext,
                                     const FrameConstructionDataByTag* aDataPtr,
                                     uint32_t aDataLength)
{
  for (const FrameConstructionDataByTag *curData = aDataPtr,
         *endData = aDataPtr + aDataLength;
       curData != endData;
       ++curData) {
    if (*curData->mTag == aTag) {
      const FrameConstructionData* data = &curData->mData;
      if (data->mBits & FCDATA_FUNC_IS_DATA_GETTER) {
        return data->mFunc.mDataGetter(aElement, aStyleContext);
      }

      return data;
    }
  }

  return nullptr;
}

#define SUPPRESS_FCDATA() FCDATA_DECL(FCDATA_SUPPRESS_FRAME, nullptr)
#define SIMPLE_INT_CREATE(_int, _func) { _int, SIMPLE_FCDATA(_func) }
#define SIMPLE_INT_CHAIN(_int, _func)                       \
  { _int, FCDATA_DECL(FCDATA_FUNC_IS_DATA_GETTER, _func) }
#define COMPLEX_INT_CREATE(_int, _func)         \
  { _int, FULL_CTOR_FCDATA(0, _func) }

#define SIMPLE_TAG_CREATE(_tag, _func)          \
  { &nsGkAtoms::_tag, SIMPLE_FCDATA(_func) }
#define SIMPLE_TAG_CHAIN(_tag, _func)                                   \
  { &nsGkAtoms::_tag, FCDATA_DECL(FCDATA_FUNC_IS_DATA_GETTER,  _func) }
#define COMPLEX_TAG_CREATE(_tag, _func)             \
  { &nsGkAtoms::_tag, FULL_CTOR_FCDATA(0, _func) }

static bool
IsFrameForFieldSet(nsIFrame* aFrame, nsIAtom* aFrameType)
{
  nsIAtom* pseudo = aFrame->StyleContext()->GetPseudo();
  if (pseudo == nsCSSAnonBoxes::fieldsetContent ||
      pseudo == nsCSSAnonBoxes::scrolledContent ||
      pseudo == nsCSSAnonBoxes::columnContent) {
    return IsFrameForFieldSet(aFrame->GetParent(), aFrame->GetParent()->GetType());
  }
  return aFrameType == nsGkAtoms::fieldSetFrame;
}

/* static */
const nsCSSFrameConstructor::FrameConstructionData*
nsCSSFrameConstructor::FindHTMLData(Element* aElement,
                                    nsIAtom* aTag,
                                    int32_t aNameSpaceID,
                                    nsIFrame* aParentFrame,
                                    nsStyleContext* aStyleContext)
{
  // Ignore the tag if it's not HTML content and if it doesn't extend (via XBL)
  // a valid HTML namespace.  This check must match the one in
  // ShouldHaveFirstLineStyle.
  if (aNameSpaceID != kNameSpaceID_XHTML) {
    return nullptr;
  }

  NS_ASSERTION(!aParentFrame ||
               aParentFrame->StyleContext()->GetPseudo() !=
                 nsCSSAnonBoxes::fieldsetContent ||
               aParentFrame->GetParent()->GetType() == nsGkAtoms::fieldSetFrame,
               "Unexpected parent for fieldset content anon box");
  if (aTag == nsGkAtoms::legend &&
      (!aParentFrame ||
       !IsFrameForFieldSet(aParentFrame, aParentFrame->GetType()) ||
       aStyleContext->StyleDisplay()->IsFloatingStyle() ||
       aStyleContext->StyleDisplay()->IsAbsolutelyPositionedStyle())) {
    // <legend> is only special inside fieldset, we only check the frame tree
    // parent because the content tree parent may not be a <fieldset> due to
    // display:contents, Shadow DOM, or XBL. For floated or absolutely
    // positioned legends we want to construct by display type and
    // not do special legend stuff.
    return nullptr;
  }

  static const FrameConstructionDataByTag sHTMLData[] = {
    SIMPLE_TAG_CHAIN(img, nsCSSFrameConstructor::FindImgData),
    SIMPLE_TAG_CHAIN(mozgeneratedcontentimage,
                     nsCSSFrameConstructor::FindImgData),
    { &nsGkAtoms::br,
      FCDATA_DECL(FCDATA_IS_LINE_PARTICIPANT | FCDATA_IS_LINE_BREAK,
                  NS_NewBRFrame) },
    SIMPLE_TAG_CREATE(wbr, NS_NewWBRFrame),
    SIMPLE_TAG_CHAIN(input, nsCSSFrameConstructor::FindInputData),
    SIMPLE_TAG_CREATE(textarea, NS_NewTextControlFrame),
    COMPLEX_TAG_CREATE(select, &nsCSSFrameConstructor::ConstructSelectFrame),
    SIMPLE_TAG_CHAIN(object, nsCSSFrameConstructor::FindObjectData),
    SIMPLE_TAG_CHAIN(applet, nsCSSFrameConstructor::FindObjectData),
    SIMPLE_TAG_CHAIN(embed, nsCSSFrameConstructor::FindObjectData),
    COMPLEX_TAG_CREATE(fieldset,
                       &nsCSSFrameConstructor::ConstructFieldSetFrame),
    { &nsGkAtoms::legend,
      FCDATA_DECL(FCDATA_ALLOW_BLOCK_STYLES | FCDATA_MAY_NEED_SCROLLFRAME,
                  NS_NewLegendFrame) },
    SIMPLE_TAG_CREATE(frameset, NS_NewHTMLFramesetFrame),
    SIMPLE_TAG_CREATE(iframe, NS_NewSubDocumentFrame),
    { &nsGkAtoms::button,
      FCDATA_WITH_WRAPPING_BLOCK(FCDATA_ALLOW_BLOCK_STYLES |
                                 FCDATA_ALLOW_GRID_FLEX_COLUMNSET,
                                 NS_NewHTMLButtonControlFrame,
                                 nsCSSAnonBoxes::buttonContent) },
    SIMPLE_TAG_CHAIN(canvas, nsCSSFrameConstructor::FindCanvasData),
    SIMPLE_TAG_CREATE(video, NS_NewHTMLVideoFrame),
    SIMPLE_TAG_CREATE(audio, NS_NewHTMLVideoFrame),
    SIMPLE_TAG_CREATE(progress, NS_NewProgressFrame),
    SIMPLE_TAG_CREATE(meter, NS_NewMeterFrame),
    COMPLEX_TAG_CREATE(details, &nsCSSFrameConstructor::ConstructDetailsFrame)
  };

  return FindDataByTag(aTag, aElement, aStyleContext, sHTMLData,
                       ArrayLength(sHTMLData));
}

/* static */
const nsCSSFrameConstructor::FrameConstructionData*
nsCSSFrameConstructor::FindImgData(Element* aElement,
                                   nsStyleContext* aStyleContext)
{
  if (!nsImageFrame::ShouldCreateImageFrameFor(aElement, aStyleContext)) {
    return nullptr;
  }

  static const FrameConstructionData sImgData = SIMPLE_FCDATA(NS_NewImageFrame);
  return &sImgData;
}

/* static */
const nsCSSFrameConstructor::FrameConstructionData*
nsCSSFrameConstructor::FindImgControlData(Element* aElement,
                                          nsStyleContext* aStyleContext)
{
  if (!nsImageFrame::ShouldCreateImageFrameFor(aElement, aStyleContext)) {
    return nullptr;
  }

  static const FrameConstructionData sImgControlData =
    SIMPLE_FCDATA(NS_NewImageControlFrame);
  return &sImgControlData;
}

/* static */
const nsCSSFrameConstructor::FrameConstructionData*
nsCSSFrameConstructor::FindInputData(Element* aElement,
                                     nsStyleContext* aStyleContext)
{
  static const FrameConstructionDataByInt sInputData[] = {
    SIMPLE_INT_CREATE(NS_FORM_INPUT_CHECKBOX, NS_NewGfxCheckboxControlFrame),
    SIMPLE_INT_CREATE(NS_FORM_INPUT_RADIO, NS_NewGfxRadioControlFrame),
    SIMPLE_INT_CREATE(NS_FORM_INPUT_FILE, NS_NewFileControlFrame),
    SIMPLE_INT_CHAIN(NS_FORM_INPUT_IMAGE,
                     nsCSSFrameConstructor::FindImgControlData),
    SIMPLE_INT_CREATE(NS_FORM_INPUT_EMAIL, NS_NewTextControlFrame),
    SIMPLE_INT_CREATE(NS_FORM_INPUT_SEARCH, NS_NewTextControlFrame),
    SIMPLE_INT_CREATE(NS_FORM_INPUT_TEXT, NS_NewTextControlFrame),
    SIMPLE_INT_CREATE(NS_FORM_INPUT_TEL, NS_NewTextControlFrame),
    SIMPLE_INT_CREATE(NS_FORM_INPUT_URL, NS_NewTextControlFrame),
    SIMPLE_INT_CREATE(NS_FORM_INPUT_RANGE, NS_NewRangeFrame),
    SIMPLE_INT_CREATE(NS_FORM_INPUT_PASSWORD, NS_NewTextControlFrame),
    { NS_FORM_INPUT_COLOR,
      FCDATA_WITH_WRAPPING_BLOCK(0, NS_NewColorControlFrame,
                                 nsCSSAnonBoxes::buttonContent) },
    // TODO: this is temporary until a frame is written: bug 635240.
    SIMPLE_INT_CREATE(NS_FORM_INPUT_NUMBER, NS_NewNumberControlFrame),
    // TODO: this is temporary until a frame is written: bug 888320.
    SIMPLE_INT_CREATE(NS_FORM_INPUT_DATE, NS_NewTextControlFrame),
#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_GONK)
    // On Android/B2G, date/time input appears as a normal text box.
    SIMPLE_INT_CREATE(NS_FORM_INPUT_TIME, NS_NewTextControlFrame),
#else
    SIMPLE_INT_CREATE(NS_FORM_INPUT_TIME, NS_NewDateTimeControlFrame),
#endif
    // TODO: this is temporary until a frame is written: bug 888320
    SIMPLE_INT_CREATE(NS_FORM_INPUT_MONTH, NS_NewTextControlFrame),
    // TODO: this is temporary until a frame is written: bug 888320
    SIMPLE_INT_CREATE(NS_FORM_INPUT_WEEK, NS_NewTextControlFrame),
    // TODO: this is temporary until a frame is written: bug 888320
    SIMPLE_INT_CREATE(NS_FORM_INPUT_DATETIME_LOCAL, NS_NewTextControlFrame),
    { NS_FORM_INPUT_SUBMIT,
      FCDATA_WITH_WRAPPING_BLOCK(0, NS_NewGfxButtonControlFrame,
                                 nsCSSAnonBoxes::buttonContent) },
    { NS_FORM_INPUT_RESET,
      FCDATA_WITH_WRAPPING_BLOCK(0, NS_NewGfxButtonControlFrame,
                                 nsCSSAnonBoxes::buttonContent) },
    { NS_FORM_INPUT_BUTTON,
      FCDATA_WITH_WRAPPING_BLOCK(0, NS_NewGfxButtonControlFrame,
                                 nsCSSAnonBoxes::buttonContent) }
    // Keeping hidden inputs out of here on purpose for so they get frames by
    // display (in practice, none).
  };

  nsCOMPtr<nsIFormControl> control = do_QueryInterface(aElement);
  NS_ASSERTION(control, "input doesn't implement nsIFormControl?");

  return FindDataByInt(control->GetType(), aElement, aStyleContext,
                       sInputData, ArrayLength(sInputData));
}

/* static */
const nsCSSFrameConstructor::FrameConstructionData*
nsCSSFrameConstructor::FindObjectData(Element* aElement,
                                      nsStyleContext* aStyleContext)
{
  // GetDisplayedType isn't necessarily nsIObjectLoadingContent::TYPE_NULL for
  // cases when the object is broken/suppressed/etc (e.g. a broken image), but
  // we want to treat those cases as TYPE_NULL
  uint32_t type;
  if (aElement->State().HasAtLeastOneOfStates(NS_EVENT_STATE_BROKEN |
                                              NS_EVENT_STATE_USERDISABLED |
                                              NS_EVENT_STATE_SUPPRESSED)) {
    type = nsIObjectLoadingContent::TYPE_NULL;
  } else {
    nsCOMPtr<nsIObjectLoadingContent> objContent(do_QueryInterface(aElement));
    NS_ASSERTION(objContent,
                 "applet, embed and object must implement "
                 "nsIObjectLoadingContent!");

    objContent->GetDisplayedType(&type);
  }

  static const FrameConstructionDataByInt sObjectData[] = {
    SIMPLE_INT_CREATE(nsIObjectLoadingContent::TYPE_LOADING,
                      NS_NewEmptyFrame),
    SIMPLE_INT_CREATE(nsIObjectLoadingContent::TYPE_PLUGIN,
                      NS_NewObjectFrame),
    SIMPLE_INT_CREATE(nsIObjectLoadingContent::TYPE_IMAGE,
                      NS_NewImageFrame),
    SIMPLE_INT_CREATE(nsIObjectLoadingContent::TYPE_DOCUMENT,
                      NS_NewSubDocumentFrame)
    // Nothing for TYPE_NULL so we'll construct frames by display there
  };

  return FindDataByInt((int32_t)type, aElement, aStyleContext,
                       sObjectData, ArrayLength(sObjectData));
}

/* static */
const nsCSSFrameConstructor::FrameConstructionData*
nsCSSFrameConstructor::FindCanvasData(Element* aElement,
                                      nsStyleContext* aStyleContext)
{
  // We want to check whether script is enabled on the document that
  // could be painting to the canvas.  That's the owner document of
  // the canvas, except when the owner document is a static document,
  // in which case it's the original document it was cloned from.
  nsIDocument* doc = aElement->OwnerDoc();
  if (doc->IsStaticDocument()) {
    doc = doc->GetOriginalDocument();
  }
  if (!doc->IsScriptEnabled()) {
    return nullptr;
  }

  static const FrameConstructionData sCanvasData =
    FCDATA_WITH_WRAPPING_BLOCK(0, NS_NewHTMLCanvasFrame,
                               nsCSSAnonBoxes::htmlCanvasContent);
  return &sCanvasData;
}

void
nsCSSFrameConstructor::ConstructFrameFromItemInternal(FrameConstructionItem& aItem,
                                                      nsFrameConstructorState& aState,
                                                      nsContainerFrame* aParentFrame,
                                                      nsFrameItems& aFrameItems)
{
  const FrameConstructionData* data = aItem.mFCData;
  NS_ASSERTION(data, "Must have frame construction data");

  uint32_t bits = data->mBits;

  NS_ASSERTION(!(bits & FCDATA_FUNC_IS_DATA_GETTER),
               "Should have dealt with this inside the data finder");

  // Some sets of bits are not compatible with each other
#define CHECK_ONLY_ONE_BIT(_bit1, _bit2)               \
  NS_ASSERTION(!(bits & _bit1) || !(bits & _bit2),     \
               "Only one of these bits should be set")
  CHECK_ONLY_ONE_BIT(FCDATA_FUNC_IS_FULL_CTOR, FCDATA_FORCE_NULL_ABSPOS_CONTAINER);
  CHECK_ONLY_ONE_BIT(FCDATA_FUNC_IS_FULL_CTOR, FCDATA_WRAP_KIDS_IN_BLOCKS);
  CHECK_ONLY_ONE_BIT(FCDATA_FUNC_IS_FULL_CTOR, FCDATA_MAY_NEED_SCROLLFRAME);
  CHECK_ONLY_ONE_BIT(FCDATA_FUNC_IS_FULL_CTOR, FCDATA_IS_POPUP);
  CHECK_ONLY_ONE_BIT(FCDATA_FUNC_IS_FULL_CTOR, FCDATA_SKIP_ABSPOS_PUSH);
  CHECK_ONLY_ONE_BIT(FCDATA_FUNC_IS_FULL_CTOR,
                     FCDATA_DISALLOW_GENERATED_CONTENT);
  CHECK_ONLY_ONE_BIT(FCDATA_FUNC_IS_FULL_CTOR, FCDATA_ALLOW_BLOCK_STYLES);
  CHECK_ONLY_ONE_BIT(FCDATA_FUNC_IS_FULL_CTOR,
                     FCDATA_CREATE_BLOCK_WRAPPER_FOR_ALL_KIDS);
  CHECK_ONLY_ONE_BIT(FCDATA_WRAP_KIDS_IN_BLOCKS,
                     FCDATA_CREATE_BLOCK_WRAPPER_FOR_ALL_KIDS);
#undef CHECK_ONLY_ONE_BIT
  NS_ASSERTION(!(bits & FCDATA_FORCED_NON_SCROLLABLE_BLOCK) ||
               ((bits & FCDATA_FUNC_IS_FULL_CTOR) &&
                data->mFullConstructor ==
                  &nsCSSFrameConstructor::ConstructNonScrollableBlock),
               "Unexpected FCDATA_FORCED_NON_SCROLLABLE_BLOCK flag");

  // Don't create a subdocument frame for iframes if we're creating extra frames
  if (aState.mCreatingExtraFrames &&
      aItem.mContent->IsHTMLElement(nsGkAtoms::iframe))
  {
    return;
  }

  nsIContent* const content = aItem.mContent;
  nsIContent* parent = content->GetParent();

  // Push display:contents ancestors.
  AutoDisplayContentsAncestorPusher adcp(aState.mTreeMatchContext,
                                         aState.mPresContext, parent);

  // Get the parent of the content and check if it is a XBL children element.
  // Push the children element as an ancestor here because it does
  // not have a frame and would not otherwise be pushed as an ancestor. It is
  // necessary to do so in order to correctly handle style resolution on
  // descendants.  (If !adcp.IsEmpty() then it was already pushed by
  // AutoDisplayContentsAncestorPusher above.)
  TreeMatchContext::AutoAncestorPusher
    insertionPointPusher(aState.mTreeMatchContext);
  if (adcp.IsEmpty() && parent && nsContentUtils::IsContentInsertionPoint(parent)) {
    if (aState.mTreeMatchContext.mAncestorFilter.HasFilter()) {
      insertionPointPusher.PushAncestorAndStyleScope(parent);
    } else {
      insertionPointPusher.PushStyleScope(parent);
    }
  }

  // Push the content as a style ancestor now, so we don't have to do
  // it in our various full-constructor functions.  In particular,
  // since a number of full-constructor functions don't actually call
  // ProcessChildren in some cases (e.g. for CSS anonymous table boxes
  // or for situations where only anonymouse children are having
  // frames constructed), this is the best place to bottleneck the
  // pushing of the content instead of having to do it in multiple
  // places.
  TreeMatchContext::AutoAncestorPusher
    ancestorPusher(aState.mTreeMatchContext);
  if (aState.mTreeMatchContext.mAncestorFilter.HasFilter()) {
    ancestorPusher.PushAncestorAndStyleScope(content);
  } else {
    ancestorPusher.PushStyleScope(content);
  }

  nsIFrame* newFrame;
  nsIFrame* primaryFrame;
  nsStyleContext* const styleContext = aItem.mStyleContext;
  const nsStyleDisplay* display = styleContext->StyleDisplay();
  if (bits & FCDATA_FUNC_IS_FULL_CTOR) {
    newFrame =
      (this->*(data->mFullConstructor))(aState, aItem, aParentFrame,
                                        display, aFrameItems);
    MOZ_ASSERT(newFrame, "Full constructor failed");
    primaryFrame = newFrame;
  } else {
    newFrame =
      (*data->mFunc.mCreationFunc)(mPresShell, styleContext);

    bool allowOutOfFlow = !(bits & FCDATA_DISALLOW_OUT_OF_FLOW);
    bool isPopup = aItem.mIsPopup;
    NS_ASSERTION(!isPopup ||
                 (aState.mPopupItems.containingBlock &&
                  aState.mPopupItems.containingBlock->GetType() ==
                    nsGkAtoms::popupSetFrame),
                 "Should have a containing block here!");

    nsContainerFrame* geometricParent =
      isPopup ? aState.mPopupItems.containingBlock :
      (allowOutOfFlow ? aState.GetGeometricParent(display, aParentFrame)
                      : aParentFrame);

    // Must init frameToAddToList to null, since it's inout
    nsIFrame* frameToAddToList = nullptr;
    if ((bits & FCDATA_MAY_NEED_SCROLLFRAME) &&
        display->IsScrollableOverflow()) {
      nsContainerFrame* scrollframe = nullptr;
      BuildScrollFrame(aState, content, styleContext, newFrame,
                       geometricParent, scrollframe);
      frameToAddToList = scrollframe;
    } else {
      InitAndRestoreFrame(aState, content, geometricParent, newFrame);
      // See whether we need to create a view
      nsContainerFrame::CreateViewForFrame(newFrame, false);
      frameToAddToList = newFrame;
    }

    // Use frameToAddToList as the primary frame.  In the non-scrollframe case
    // they're equal, but in the scrollframe case newFrame is the scrolled
    // frame, while frameToAddToList is the scrollframe (and should be the
    // primary frame).
    primaryFrame = frameToAddToList;

    // If we need to create a block formatting context to wrap our
    // kids, do it now.
    const nsStyleDisplay* maybeAbsoluteContainingBlockDisplay = display;
    nsIFrame* maybeAbsoluteContainingBlockStyleFrame = primaryFrame;
    nsIFrame* maybeAbsoluteContainingBlock = newFrame;
    nsIFrame* possiblyLeafFrame = newFrame;
    if (bits & FCDATA_CREATE_BLOCK_WRAPPER_FOR_ALL_KIDS) {
      RefPtr<nsStyleContext> outerSC =
        mPresShell->StyleSet()->ResolveAnonymousBoxStyle(*data->mAnonBoxPseudo,
                                                         styleContext);
#ifdef DEBUG
      nsContainerFrame* containerFrame = do_QueryFrame(newFrame);
      MOZ_ASSERT(containerFrame);
#endif
      nsContainerFrame* container = static_cast<nsContainerFrame*>(newFrame);
      nsContainerFrame* outerFrame;
      nsContainerFrame* innerFrame;
      if (bits & FCDATA_ALLOW_GRID_FLEX_COLUMNSET) {
        switch (display->mDisplay) {
          case StyleDisplay::Flex:
          case StyleDisplay::InlineFlex:
            outerFrame = NS_NewFlexContainerFrame(mPresShell, outerSC);
            InitAndRestoreFrame(aState, content, container, outerFrame);
            innerFrame = outerFrame;
            break;
          case StyleDisplay::Grid:
          case StyleDisplay::InlineGrid:
            outerFrame = NS_NewGridContainerFrame(mPresShell, outerSC);
            InitAndRestoreFrame(aState, content, container, outerFrame);
            innerFrame = outerFrame;
            break;
          default: {
            nsContainerFrame* columnSetFrame = nullptr;
            RefPtr<nsStyleContext> innerSC = outerSC;
            const nsStyleColumn* columns = outerSC->StyleColumn();
            if (columns->mColumnCount != NS_STYLE_COLUMN_COUNT_AUTO ||
                columns->mColumnWidth.GetUnit() != eStyleUnit_Auto) {
              columnSetFrame =
                NS_NewColumnSetFrame(mPresShell, outerSC, nsFrameState(0));
              InitAndRestoreFrame(aState, content, container, columnSetFrame);
              innerSC = mPresShell->StyleSet()->ResolveAnonymousBoxStyle(
                nsCSSAnonBoxes::columnContent, outerSC);
            }
            innerFrame = NS_NewBlockFormattingContext(mPresShell, innerSC);
            if (columnSetFrame) {
              InitAndRestoreFrame(aState, content, columnSetFrame, innerFrame);
              SetInitialSingleChild(columnSetFrame, innerFrame);
              outerFrame = columnSetFrame;
            } else {
              InitAndRestoreFrame(aState, content, container, innerFrame);
              outerFrame = innerFrame;
            }
            break;
          }
        }
      } else {
        innerFrame = NS_NewBlockFormattingContext(mPresShell, outerSC);
        InitAndRestoreFrame(aState, content, container, innerFrame);
        outerFrame = innerFrame;
      }

      SetInitialSingleChild(container, outerFrame);

      // Now figure out whether newFrame or outerFrame should be the
      // absolute container.
      auto outerDisplay = outerSC->StyleDisplay();
      if (outerDisplay->IsAbsPosContainingBlock(outerFrame)) {
        maybeAbsoluteContainingBlockDisplay = outerDisplay;
        maybeAbsoluteContainingBlock = outerFrame;
        maybeAbsoluteContainingBlockStyleFrame = outerFrame;
        innerFrame->AddStateBits(NS_FRAME_CAN_HAVE_ABSPOS_CHILDREN);
      }

      // Our kids should go into the innerFrame.
      newFrame = innerFrame;
    }

    aState.AddChild(frameToAddToList, aFrameItems, content, styleContext,
                    aParentFrame, allowOutOfFlow, allowOutOfFlow, isPopup);

    nsContainerFrame* newFrameAsContainer = do_QueryFrame(newFrame);
    if (newFrameAsContainer) {
#ifdef MOZ_XUL
      // Icky XUL stuff, sadly

      if (aItem.mIsRootPopupgroup) {
        NS_ASSERTION(nsIRootBox::GetRootBox(mPresShell) &&
                     nsIRootBox::GetRootBox(mPresShell)->GetPopupSetFrame() ==
                     newFrame,
                     "Unexpected PopupSetFrame");
        aState.mPopupItems.containingBlock = newFrameAsContainer;
        aState.mHavePendingPopupgroup = false;
      }
#endif /* MOZ_XUL */

      // Process the child content if requested
      nsFrameItems childItems;
      nsFrameConstructorSaveState absoluteSaveState;

      if (bits & FCDATA_FORCE_NULL_ABSPOS_CONTAINER) {
        aState.PushAbsoluteContainingBlock(nullptr, nullptr, absoluteSaveState);
      } else if (!(bits & FCDATA_SKIP_ABSPOS_PUSH)) {
        maybeAbsoluteContainingBlock->AddStateBits(NS_FRAME_CAN_HAVE_ABSPOS_CHILDREN);
        // This check is identical to nsStyleDisplay::IsAbsPosContainingBlock
        // except without the assertion that the style display and frame match.
        // When constructing scroll frames we intentionally use the style
        // display for the outer, but make the inner the containing block.
        if ((maybeAbsoluteContainingBlockDisplay->IsAbsolutelyPositionedStyle() ||
             maybeAbsoluteContainingBlockDisplay->IsRelativelyPositionedStyle() ||
             maybeAbsoluteContainingBlockDisplay->IsFixedPosContainingBlock(
                 maybeAbsoluteContainingBlockStyleFrame)) &&
            !maybeAbsoluteContainingBlockStyleFrame->IsSVGText()) {
          nsContainerFrame* cf = static_cast<nsContainerFrame*>(
              maybeAbsoluteContainingBlock);
          aState.PushAbsoluteContainingBlock(cf, cf, absoluteSaveState);
        }
      }

      if (!aItem.mAnonChildren.IsEmpty()) {
        NS_ASSERTION(!(bits & FCDATA_USE_CHILD_ITEMS),
                     "We should not have both anonymous and non-anonymous "
                     "children in a given FrameConstructorItem");
        AddFCItemsForAnonymousContent(aState, newFrameAsContainer, aItem.mAnonChildren,
                                      aItem.mChildItems);
        bits |= FCDATA_USE_CHILD_ITEMS;
      }

      if (bits & FCDATA_USE_CHILD_ITEMS) {
        nsFrameConstructorSaveState floatSaveState;

        if (ShouldSuppressFloatingOfDescendants(newFrame)) {
          aState.PushFloatContainingBlock(nullptr, floatSaveState);
        } else if (newFrame->IsFloatContainingBlock()) {
          aState.PushFloatContainingBlock(newFrameAsContainer, floatSaveState);
        }
        ConstructFramesFromItemList(aState, aItem.mChildItems, newFrameAsContainer,
                                    childItems);
      } else {
        // Process the child frames.
        ProcessChildren(aState, content, styleContext, newFrameAsContainer,
                        !(bits & FCDATA_DISALLOW_GENERATED_CONTENT),
                        childItems,
                        (bits & FCDATA_ALLOW_BLOCK_STYLES) != 0,
                        aItem.mPendingBinding, possiblyLeafFrame);
      }

      if (bits & FCDATA_WRAP_KIDS_IN_BLOCKS) {
        nsFrameItems newItems;
        nsFrameItems currentBlockItems;
        nsIFrame* f;
        while ((f = childItems.FirstChild()) != nullptr) {
          bool wrapFrame = IsInlineFrame(f) || IsFramePartOfIBSplit(f);
          if (!wrapFrame) {
            FlushAccumulatedBlock(aState, content, newFrameAsContainer,
                                  currentBlockItems, newItems);
          }

          childItems.RemoveFrame(f);
          if (wrapFrame) {
            currentBlockItems.AddChild(f);
          } else {
            newItems.AddChild(f);
          }
        }
        FlushAccumulatedBlock(aState, content, newFrameAsContainer,
                              currentBlockItems, newItems);

        if (childItems.NotEmpty()) {
          // an error must have occurred, delete unprocessed frames
          childItems.DestroyFrames();
        }

        childItems = newItems;
      }

      // Set the frame's initial child list
      // Note that MathML depends on this being called even if
      // childItems is empty!
      newFrameAsContainer->SetInitialChildList(kPrincipalList, childItems);
    }
  }

#ifdef MOZ_XUL
  // More icky XUL stuff
  if (aItem.mNameSpaceID == kNameSpaceID_XUL &&
      (aItem.mTag == nsGkAtoms::treechildren || // trees always need titletips
       content->HasAttr(kNameSpaceID_None, nsGkAtoms::tooltiptext) ||
       content->HasAttr(kNameSpaceID_None, nsGkAtoms::tooltip))) {
    nsIRootBox* rootBox = nsIRootBox::GetRootBox(mPresShell);
    if (rootBox) {
      rootBox->AddTooltipSupport(content);
    }
  }
#endif

  NS_ASSERTION(newFrame->IsFrameOfType(nsIFrame::eLineParticipant) ==
               ((bits & FCDATA_IS_LINE_PARTICIPANT) != 0),
               "Incorrectly set FCDATA_IS_LINE_PARTICIPANT bits");

  if (aItem.mIsAnonymousContentCreatorContent) {
    primaryFrame->AddStateBits(NS_FRAME_ANONYMOUSCONTENTCREATOR_CONTENT);
  }

  // Even if mCreatingExtraFrames is set, we may need to SetPrimaryFrame for
  // generated content that doesn't have one yet.  Note that we have to examine
  // the frame bit, because by this point mIsGeneratedContent has been cleared
  // on aItem.
  if ((!aState.mCreatingExtraFrames ||
       (primaryFrame->HasAnyStateBits(NS_FRAME_ANONYMOUSCONTENTCREATOR_CONTENT |
                                      NS_FRAME_GENERATED_CONTENT) &&
        !aItem.mContent->GetPrimaryFrame())) &&
       !(bits & FCDATA_SKIP_FRAMESET)) {
    aItem.mContent->SetPrimaryFrame(primaryFrame);
    ActiveLayerTracker::TransferActivityToFrame(aItem.mContent, primaryFrame);
  }
}

// after the node has been constructed and initialized create any
// anonymous content a node needs.
nsresult
nsCSSFrameConstructor::CreateAnonymousFrames(nsFrameConstructorState& aState,
                                             nsIContent*              aParent,
                                             nsContainerFrame*        aParentFrame,
                                             PendingBinding*          aPendingBinding,
                                             nsFrameItems&            aChildItems)
{
  AutoTArray<nsIAnonymousContentCreator::ContentInfo, 4> newAnonymousItems;
  nsresult rv = GetAnonymousContent(aParent, aParentFrame, newAnonymousItems);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t count = newAnonymousItems.Length();
  if (count == 0) {
    return NS_OK;
  }

  nsFrameConstructorState::PendingBindingAutoPusher pusher(aState,
                                                           aPendingBinding);
  TreeMatchContext::AutoAncestorPusher ancestorPusher(aState.mTreeMatchContext);
  if (aState.mTreeMatchContext.mAncestorFilter.HasFilter()) {
    ancestorPusher.PushAncestorAndStyleScope(aParent->AsElement());
  } else {
    ancestorPusher.PushStyleScope(aParent->AsElement());
  }

  nsIAnonymousContentCreator* creator = do_QueryFrame(aParentFrame);
  NS_ASSERTION(creator,
               "How can that happen if we have nodes to construct frames for?");

  InsertionPoint insertion(aParentFrame, aParent);
  for (uint32_t i=0; i < count; i++) {
    nsIContent* content = newAnonymousItems[i].mContent;
    NS_ASSERTION(content, "null anonymous content?");
    NS_ASSERTION(!newAnonymousItems[i].mStyleContext, "Unexpected style context");
    NS_ASSERTION(newAnonymousItems[i].mChildren.IsEmpty(),
                 "This method is not currently used with frames that implement "
                 "nsIAnonymousContentCreator::CreateAnonymousContent to "
                 "output a list where the items have their own children");

    nsIFrame* newFrame = creator->CreateFrameFor(content);
    if (newFrame) {
      NS_ASSERTION(content->GetPrimaryFrame(),
                   "Content must have a primary frame now");
      newFrame->AddStateBits(NS_FRAME_ANONYMOUSCONTENTCREATOR_CONTENT);
      aChildItems.AddChild(newFrame);
    } else {
      FrameConstructionItemList items;
      {
        // Skip parent display based style-fixup during our
        // AddFrameConstructionItems() call:
        TreeMatchContext::AutoParentDisplayBasedStyleFixupSkipper
          parentDisplayBasedStyleFixupSkipper(aState.mTreeMatchContext);

        AddFrameConstructionItems(aState, content, true, insertion, items);
      }
      ConstructFramesFromItemList(aState, items, aParentFrame, aChildItems);
    }
  }

  return NS_OK;
}

static void
SetFlagsOnSubtree(nsIContent *aNode, uintptr_t aFlagsToSet)
{
#ifdef DEBUG
  // Make sure that the node passed to us doesn't have any XBL children
  {
    FlattenedChildIterator iter(aNode);
    NS_ASSERTION(!iter.XBLInvolved() || !iter.GetNextChild(),
                 "The node should not have any XBL children");
  }
#endif

  // Set the flag on the node itself
  aNode->SetFlags(aFlagsToSet);

  // Set the flag on all of its children recursively
  uint32_t count;
  nsIContent * const *children = aNode->GetChildArray(&count);

  for (uint32_t index = 0; index < count; ++index) {
    SetFlagsOnSubtree(children[index], aFlagsToSet);
  }
}

/**
 * This function takes a tree of nsIAnonymousContentCreator::ContentInfo
 * objects where the nsIContent nodes have just been created, and appends the
 * nsIContent children in the tree to their parent. The leaf nsIContent objects
 * are appended first to minimize the number of notifications that are sent
 * out (i.e. by appending as many descendants as posible while their parent is
 * not yet in the document tree).
 *
 * This function is used simply as a convenience so that implementations of
 * nsIAnonymousContentCreator::CreateAnonymousContent don't all have to have
 * their own code to connect the elements that they create.
 */
static void
ConnectAnonymousTreeDescendants(nsIContent* aParent,
                                nsTArray<nsIAnonymousContentCreator::ContentInfo>& aContent)
{
  uint32_t count = aContent.Length();
  for (uint32_t i=0; i < count; i++) {
    nsIContent* content = aContent[i].mContent;
    NS_ASSERTION(content, "null anonymous content?");

    ConnectAnonymousTreeDescendants(content, aContent[i].mChildren);

    aParent->AppendChildTo(content, false);
  }
}

nsresult
nsCSSFrameConstructor::GetAnonymousContent(nsIContent* aParent,
                                           nsIFrame* aParentFrame,
                                           nsTArray<nsIAnonymousContentCreator::ContentInfo>& aContent)
{
  nsIAnonymousContentCreator* creator = do_QueryFrame(aParentFrame);
  if (!creator)
    return NS_OK;

  nsresult rv = creator->CreateAnonymousContent(aContent);
  if (NS_FAILED(rv)) {
    // CreateAnonymousContent failed, e.g. because the page has a <use> loop.
    return rv;
  }

  uint32_t count = aContent.Length();
  for (uint32_t i=0; i < count; i++) {
    // get our child's content and set its parent to our content
    nsIContent* content = aContent[i].mContent;
    NS_ASSERTION(content, "null anonymous content?");

    // least-surprise CSS binding until we do the SVG specified
    // cascading rules for <svg:use> - bug 265894
    if (aParentFrame->GetType() == nsGkAtoms::svgUseFrame) {
      content->SetFlags(NODE_IS_ANONYMOUS_ROOT);
    } else {
      content->SetIsNativeAnonymousRoot();
    }

    ConnectAnonymousTreeDescendants(content, aContent[i].mChildren);

    bool anonContentIsEditable = content->HasFlag(NODE_IS_EDITABLE);

    // If the parent is in a shadow tree, make sure we don't
    // bind with a document because shadow roots and its descendants
    // are not in document.
    nsIDocument* bindDocument =
      aParent->HasFlag(NODE_IS_IN_SHADOW_TREE) ? nullptr : mDocument;
    rv = content->BindToTree(bindDocument, aParent, aParent, true);
    // If the anonymous content creator requested that the content should be
    // editable, honor its request.
    // We need to set the flag on the whole subtree, because existing
    // children's flags have already been set as part of the BindToTree operation.
    if (anonContentIsEditable) {
      NS_ASSERTION(aParentFrame->GetType() == nsGkAtoms::textInputFrame,
                   "We only expect this for anonymous content under a text control frame");
      SetFlagsOnSubtree(content, NODE_IS_EDITABLE);
    }
    if (NS_FAILED(rv)) {
      content->UnbindFromTree();
      return rv;
    }
  }

  if (ServoStyleSet* styleSet = mPresShell->StyleSet()->GetAsServo()) {
    // Eagerly compute styles for the anonymous content tree, but only do so
    // if the content doesn't have an explicit style context (if it does, we
    // don't need the normal computed values).
    for (auto& info : aContent) {
      if (!info.mStyleContext) {
        styleSet->StyleNewSubtree(info.mContent);
      }
    }
  }

  return NS_OK;
}

static
bool IsXULDisplayType(const nsStyleDisplay* aDisplay)
{
  return (aDisplay->mDisplay == StyleDisplay::InlineBox ||
#ifdef MOZ_XUL
          aDisplay->mDisplay == StyleDisplay::InlineXulGrid ||
          aDisplay->mDisplay == StyleDisplay::InlineStack ||
#endif
          aDisplay->mDisplay == StyleDisplay::Box
#ifdef MOZ_XUL
          || aDisplay->mDisplay == StyleDisplay::XulGrid ||
          aDisplay->mDisplay == StyleDisplay::Stack ||
          aDisplay->mDisplay == StyleDisplay::XulGridGroup ||
          aDisplay->mDisplay == StyleDisplay::XulGridLine ||
          aDisplay->mDisplay == StyleDisplay::Deck ||
          aDisplay->mDisplay == StyleDisplay::Popup ||
          aDisplay->mDisplay == StyleDisplay::Groupbox
#endif
          );
}


// XUL frames are not allowed to be out of flow.
#define SIMPLE_XUL_FCDATA(_func)                                        \
  FCDATA_DECL(FCDATA_DISALLOW_OUT_OF_FLOW | FCDATA_SKIP_ABSPOS_PUSH,    \
              _func)
#define SCROLLABLE_XUL_FCDATA(_func)                                    \
  FCDATA_DECL(FCDATA_DISALLOW_OUT_OF_FLOW | FCDATA_SKIP_ABSPOS_PUSH |   \
              FCDATA_MAY_NEED_SCROLLFRAME, _func)
// .. but we allow some XUL frames to be _containers_ for out-of-flow content
// (This is the same as SCROLLABLE_XUL_FCDATA, but w/o FCDATA_SKIP_ABSPOS_PUSH)
#define SCROLLABLE_ABSPOS_CONTAINER_XUL_FCDATA(_func)                   \
  FCDATA_DECL(FCDATA_DISALLOW_OUT_OF_FLOW |                             \
              FCDATA_MAY_NEED_SCROLLFRAME, _func)

#define SIMPLE_XUL_CREATE(_tag, _func)            \
  { &nsGkAtoms::_tag, SIMPLE_XUL_FCDATA(_func) }
#define SCROLLABLE_XUL_CREATE(_tag, _func)            \
  { &nsGkAtoms::_tag, SCROLLABLE_XUL_FCDATA(_func) }
#define SIMPLE_XUL_DISPLAY_CREATE(_display, _func)      \
  FCDATA_FOR_DISPLAY(_display, SIMPLE_XUL_FCDATA(_func))
#define SCROLLABLE_XUL_DISPLAY_CREATE(_display, _func)                          \
  FCDATA_FOR_DISPLAY(_display, SCROLLABLE_XUL_FCDATA(_func))
#define SCROLLABLE_ABSPOS_CONTAINER_XUL_DISPLAY_CREATE(_display, _func)         \
  FCDATA_FOR_DISPLAY(_display, SCROLLABLE_ABSPOS_CONTAINER_XUL_FCDATA(_func))

static
nsIFrame* NS_NewGridBoxFrame(nsIPresShell* aPresShell,
                             nsStyleContext* aStyleContext)
{
  nsCOMPtr<nsBoxLayout> layout;
  NS_NewGridLayout2(aPresShell, getter_AddRefs(layout));
  return NS_NewBoxFrame(aPresShell, aStyleContext, false, layout);
}

/* static */
const nsCSSFrameConstructor::FrameConstructionData*
nsCSSFrameConstructor::FindXULTagData(Element* aElement,
                                      nsIAtom* aTag,
                                      int32_t aNameSpaceID,
                                      nsStyleContext* aStyleContext)
{
  if (aNameSpaceID != kNameSpaceID_XUL) {
    return nullptr;
  }

  static const FrameConstructionDataByTag sXULTagData[] = {
#ifdef MOZ_XUL
    SCROLLABLE_XUL_CREATE(button, NS_NewButtonBoxFrame),
    SCROLLABLE_XUL_CREATE(checkbox, NS_NewButtonBoxFrame),
    SCROLLABLE_XUL_CREATE(radio, NS_NewButtonBoxFrame),
    SCROLLABLE_XUL_CREATE(autorepeatbutton, NS_NewAutoRepeatBoxFrame),
    SCROLLABLE_XUL_CREATE(titlebar, NS_NewTitleBarFrame),
    SCROLLABLE_XUL_CREATE(resizer, NS_NewResizerFrame),
    SIMPLE_XUL_CREATE(image, NS_NewImageBoxFrame),
    SIMPLE_XUL_CREATE(spring, NS_NewLeafBoxFrame),
    SIMPLE_XUL_CREATE(spacer, NS_NewLeafBoxFrame),
    SIMPLE_XUL_CREATE(treechildren, NS_NewTreeBodyFrame),
    SIMPLE_XUL_CREATE(treecol, NS_NewTreeColFrame),
    SIMPLE_XUL_CREATE(text, NS_NewTextBoxFrame),
    SIMPLE_TAG_CHAIN(label, nsCSSFrameConstructor::FindXULLabelData),
    SIMPLE_TAG_CHAIN(description, nsCSSFrameConstructor::FindXULDescriptionData),
    SIMPLE_XUL_CREATE(menu, NS_NewMenuFrame),
    SIMPLE_XUL_CREATE(menubutton, NS_NewMenuFrame),
    SIMPLE_XUL_CREATE(menuitem, NS_NewMenuItemFrame),
#ifdef XP_MACOSX
    SIMPLE_TAG_CHAIN(menubar, nsCSSFrameConstructor::FindXULMenubarData),
#else
    SIMPLE_XUL_CREATE(menubar, NS_NewMenuBarFrame),
#endif /* XP_MACOSX */
    SIMPLE_TAG_CHAIN(popupgroup, nsCSSFrameConstructor::FindPopupGroupData),
    SIMPLE_XUL_CREATE(iframe, NS_NewSubDocumentFrame),
    SIMPLE_XUL_CREATE(editor, NS_NewSubDocumentFrame),
    SIMPLE_XUL_CREATE(browser, NS_NewSubDocumentFrame),
    SIMPLE_XUL_CREATE(progressmeter, NS_NewProgressMeterFrame),
    SIMPLE_XUL_CREATE(splitter, NS_NewSplitterFrame),
    SIMPLE_TAG_CHAIN(listboxbody,
                     nsCSSFrameConstructor::FindXULListBoxBodyData),
    SIMPLE_TAG_CHAIN(listitem, nsCSSFrameConstructor::FindXULListItemData),
#endif /* MOZ_XUL */
    SIMPLE_XUL_CREATE(slider, NS_NewSliderFrame),
    SIMPLE_XUL_CREATE(scrollbar, NS_NewScrollbarFrame),
    SIMPLE_XUL_CREATE(scrollbarbutton, NS_NewScrollbarButtonFrame)
};

  return FindDataByTag(aTag, aElement, aStyleContext, sXULTagData,
                       ArrayLength(sXULTagData));
}

#ifdef MOZ_XUL
/* static */
const nsCSSFrameConstructor::FrameConstructionData*
nsCSSFrameConstructor::FindPopupGroupData(Element* aElement,
                                          nsStyleContext* /* unused */)
{
  if (!aElement->IsRootOfNativeAnonymousSubtree()) {
    return nullptr;
  }

  static const FrameConstructionData sPopupSetData =
    SIMPLE_XUL_FCDATA(NS_NewPopupSetFrame);
  return &sPopupSetData;
}

/* static */
const nsCSSFrameConstructor::FrameConstructionData
nsCSSFrameConstructor::sXULTextBoxData = SIMPLE_XUL_FCDATA(NS_NewTextBoxFrame);

/* static */
const nsCSSFrameConstructor::FrameConstructionData*
nsCSSFrameConstructor::FindXULLabelData(Element* aElement,
                                        nsStyleContext* /* unused */)
{
  if (aElement->HasAttr(kNameSpaceID_None, nsGkAtoms::value)) {
    return &sXULTextBoxData;
  }

  static const FrameConstructionData sLabelData =
    SIMPLE_XUL_FCDATA(NS_NewXULLabelFrame);
  return &sLabelData;
}

static nsIFrame*
NS_NewXULDescriptionFrame(nsIPresShell* aPresShell, nsStyleContext *aContext)
{
  // XXXbz do we really need to set up the block formatting context root? If the
  // parent is not a block we'll get it anyway, and if it is, do we want it?
  return NS_NewBlockFormattingContext(aPresShell, aContext);
}

/* static */
const nsCSSFrameConstructor::FrameConstructionData*
nsCSSFrameConstructor::FindXULDescriptionData(Element* aElement,
                                              nsStyleContext* /* unused */)
{
  if (aElement->HasAttr(kNameSpaceID_None, nsGkAtoms::value)) {
    return &sXULTextBoxData;
  }

  static const FrameConstructionData sDescriptionData =
    SIMPLE_XUL_FCDATA(NS_NewXULDescriptionFrame);
  return &sDescriptionData;
}

#ifdef XP_MACOSX
/* static */
const nsCSSFrameConstructor::FrameConstructionData*
nsCSSFrameConstructor::FindXULMenubarData(Element* aElement,
                                          nsStyleContext* aStyleContext)
{
  nsCOMPtr<nsIDocShell> treeItem =
    aStyleContext->PresContext()->GetDocShell();
  if (treeItem && nsIDocShellTreeItem::typeChrome == treeItem->ItemType()) {
    nsCOMPtr<nsIDocShellTreeItem> parent;
    treeItem->GetParent(getter_AddRefs(parent));
    if (!parent) {
      // This is the root.  Suppress the menubar, since on Mac
      // window menus are not attached to the window.
      static const FrameConstructionData sSuppressData = SUPPRESS_FCDATA();
      return &sSuppressData;
    }
  }

  static const FrameConstructionData sMenubarData =
    SIMPLE_XUL_FCDATA(NS_NewMenuBarFrame);
  return &sMenubarData;
}
#endif /* XP_MACOSX */

/* static */
const nsCSSFrameConstructor::FrameConstructionData*
nsCSSFrameConstructor::FindXULListBoxBodyData(Element* aElement,
                                              nsStyleContext* aStyleContext)
{
  if (aStyleContext->StyleDisplay()->mDisplay !=
        StyleDisplay::XulGridGroup) {
    return nullptr;
  }

  static const FrameConstructionData sListBoxBodyData =
    SCROLLABLE_XUL_FCDATA(NS_NewListBoxBodyFrame);
  return &sListBoxBodyData;
}

/* static */
const nsCSSFrameConstructor::FrameConstructionData*
nsCSSFrameConstructor::FindXULListItemData(Element* aElement,
                                           nsStyleContext* aStyleContext)
{
  if (aStyleContext->StyleDisplay()->mDisplay != StyleDisplay::XulGridLine) {
    return nullptr;
  }

  static const FrameConstructionData sListItemData =
    SCROLLABLE_XUL_FCDATA(NS_NewListItemFrame);
  return &sListItemData;
}

#endif /* MOZ_XUL */

/* static */
const nsCSSFrameConstructor::FrameConstructionData*
nsCSSFrameConstructor::FindXULDisplayData(const nsStyleDisplay* aDisplay,
                                          Element* aElement,
                                          nsStyleContext* aStyleContext)
{
  static const FrameConstructionDataByDisplay sXULDisplayData[] = {
    SCROLLABLE_ABSPOS_CONTAINER_XUL_DISPLAY_CREATE(StyleDisplay::Box,
                                                   NS_NewBoxFrame),
    SCROLLABLE_ABSPOS_CONTAINER_XUL_DISPLAY_CREATE(StyleDisplay::InlineBox,
                                                   NS_NewBoxFrame),
#ifdef MOZ_XUL
    SCROLLABLE_XUL_DISPLAY_CREATE(StyleDisplay::XulGrid, NS_NewGridBoxFrame),
    SCROLLABLE_XUL_DISPLAY_CREATE(StyleDisplay::InlineXulGrid, NS_NewGridBoxFrame),
    SCROLLABLE_XUL_DISPLAY_CREATE(StyleDisplay::XulGridGroup,
                                  NS_NewGridRowGroupFrame),
    SCROLLABLE_XUL_DISPLAY_CREATE(StyleDisplay::XulGridLine,
                                  NS_NewGridRowLeafFrame),
    SCROLLABLE_XUL_DISPLAY_CREATE(StyleDisplay::Stack, NS_NewStackFrame),
    SCROLLABLE_XUL_DISPLAY_CREATE(StyleDisplay::InlineStack, NS_NewStackFrame),
    SIMPLE_XUL_DISPLAY_CREATE(StyleDisplay::Deck, NS_NewDeckFrame),
    SCROLLABLE_XUL_DISPLAY_CREATE(StyleDisplay::Groupbox, NS_NewGroupBoxFrame),
    FCDATA_FOR_DISPLAY(StyleDisplay::Popup,
      FCDATA_DECL(FCDATA_DISALLOW_OUT_OF_FLOW | FCDATA_IS_POPUP |
                  FCDATA_SKIP_ABSPOS_PUSH, NS_NewMenuPopupFrame))
#endif /* MOZ_XUL */
  };

  if (aDisplay->mDisplay < StyleDisplay::Box) {
    return nullptr;
  }

  MOZ_ASSERT(aDisplay->mDisplay <= StyleDisplay::Popup,
             "Someone added a new display value?");

  const FrameConstructionDataByDisplay& data =
    sXULDisplayData[size_t(aDisplay->mDisplay) - size_t(StyleDisplay::Box)];
  MOZ_ASSERT(aDisplay->mDisplay == data.mDisplay,
             "Did someone mess with the order?");

  return &data.mData;
}

already_AddRefed<nsStyleContext>
nsCSSFrameConstructor::BeginBuildingScrollFrame(nsFrameConstructorState& aState,
                                                nsIContent*              aContent,
                                                nsStyleContext*          aContentStyle,
                                                nsContainerFrame*        aParentFrame,
                                                nsIAtom*                 aScrolledPseudo,
                                                bool                     aIsRoot,
                                                nsContainerFrame*&       aNewFrame)
{
  nsContainerFrame* gfxScrollFrame = aNewFrame;

  nsFrameItems anonymousItems;

  RefPtr<nsStyleContext> contentStyle = aContentStyle;

  if (!gfxScrollFrame) {
    // Build a XULScrollFrame when the child is a box, otherwise an
    // HTMLScrollFrame
    // XXXbz this is the lone remaining consumer of IsXULDisplayType.
    // I wonder whether we can eliminate that somehow.
    const nsStyleDisplay* displayStyle = aContentStyle->StyleDisplay();
    if (IsXULDisplayType(displayStyle)) {
      gfxScrollFrame = NS_NewXULScrollFrame(mPresShell, contentStyle, aIsRoot,
          displayStyle->mDisplay == StyleDisplay::Stack ||
          displayStyle->mDisplay == StyleDisplay::InlineStack);
    } else {
      gfxScrollFrame = NS_NewHTMLScrollFrame(mPresShell, contentStyle, aIsRoot);
    }

    InitAndRestoreFrame(aState, aContent, aParentFrame, gfxScrollFrame);
  }

  // if there are any anonymous children for the scroll frame, create
  // frames for them.
  // Pass a null pending binding: we don't care how constructors for any of
  // this anonymous content order with anything else.  It's never been
  // consistent anyway.
  CreateAnonymousFrames(aState, aContent, gfxScrollFrame, nullptr,
                        anonymousItems);

  aNewFrame = gfxScrollFrame;

  // we used the style that was passed in. So resolve another one.
  StyleSetHandle styleSet = mPresShell->StyleSet();
  RefPtr<nsStyleContext> scrolledChildStyle =
    styleSet->ResolveAnonymousBoxStyle(aScrolledPseudo, contentStyle);

  if (gfxScrollFrame) {
     gfxScrollFrame->SetInitialChildList(kPrincipalList, anonymousItems);
  }

  return scrolledChildStyle.forget();
}

void
nsCSSFrameConstructor::FinishBuildingScrollFrame(nsContainerFrame* aScrollFrame,
                                                 nsIFrame* aScrolledFrame)
{
  nsFrameList scrolled(aScrolledFrame, aScrolledFrame);
  aScrollFrame->AppendFrames(kPrincipalList, scrolled);
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
void
nsCSSFrameConstructor::BuildScrollFrame(nsFrameConstructorState& aState,
                                        nsIContent*              aContent,
                                        nsStyleContext*          aContentStyle,
                                        nsIFrame*                aScrolledFrame,
                                        nsContainerFrame*        aParentFrame,
                                        nsContainerFrame*&       aNewFrame)
{
  RefPtr<nsStyleContext> scrolledContentStyle =
    BeginBuildingScrollFrame(aState, aContent, aContentStyle, aParentFrame,
                             nsCSSAnonBoxes::scrolledContent,
                             false, aNewFrame);

  aScrolledFrame->SetStyleContextWithoutNotification(scrolledContentStyle);
  InitAndRestoreFrame(aState, aContent, aNewFrame, aScrolledFrame);

  FinishBuildingScrollFrame(aNewFrame, aScrolledFrame);
}

const nsCSSFrameConstructor::FrameConstructionData*
nsCSSFrameConstructor::FindDisplayData(const nsStyleDisplay* aDisplay,
                                       Element* aElement,
                                       nsStyleContext* aStyleContext)
{
  static_assert(eParentTypeCount < (1 << (32 - FCDATA_PARENT_TYPE_OFFSET)),
                "Check eParentTypeCount should not overflow");

  // The style system ensures that floated and positioned frames are
  // block-level.
  NS_ASSERTION(!(aDisplay->IsFloatingStyle() ||
                 aDisplay->IsAbsolutelyPositionedStyle()) ||
               aDisplay->IsBlockOutsideStyle() ||
               aDisplay->mDisplay == StyleDisplay::Contents,
               "Style system did not apply CSS2.1 section 9.7 fixups");

  // If this is "body", try propagating its scroll style to the viewport
  // Note that we need to do this even if the body is NOT scrollable;
  // it might have dynamically changed from scrollable to not scrollable,
  // and that might need to be propagated.
  // XXXbz is this the right place to do this?  If this code moves,
  // make this function static.
  bool propagatedScrollToViewport = false;
  if (aElement->IsHTMLElement(nsGkAtoms::body)) {
    if (nsPresContext* presContext = mPresShell->GetPresContext()) {
      propagatedScrollToViewport =
        presContext->UpdateViewportScrollbarStylesOverride() == aElement;
    }
  }

  NS_ASSERTION(!propagatedScrollToViewport ||
               !mPresShell->GetPresContext()->IsPaginated(),
               "Shouldn't propagate scroll in paginated contexts");

  if (aDisplay->IsBlockInsideStyle()) {
    // If the frame is a block-level frame and is scrollable, then wrap it in a
    // scroll frame.  Except we don't want to do that for paginated contexts for
    // frames that are block-outside and aren't frames for native anonymous stuff.
    // XXX Ignore tables for the time being (except caption)
    const uint32_t kCaptionCtorFlags =
      FCDATA_IS_TABLE_PART | FCDATA_DESIRED_PARENT_TYPE_TO_BITS(eTypeTable);
    bool caption = aDisplay->mDisplay == StyleDisplay::TableCaption;
    bool suppressScrollFrame = false;
    bool needScrollFrame = aDisplay->IsScrollableOverflow() &&
                           !propagatedScrollToViewport;
    if (needScrollFrame) {
      suppressScrollFrame = mPresShell->GetPresContext()->IsPaginated() &&
                            aDisplay->IsBlockOutsideStyle() &&
                            !aElement->IsInNativeAnonymousSubtree();
      if (!suppressScrollFrame) {
        static const FrameConstructionData sScrollableBlockData[2] =
          { FULL_CTOR_FCDATA(0, &nsCSSFrameConstructor::ConstructScrollableBlock),
            FULL_CTOR_FCDATA(kCaptionCtorFlags,
                             &nsCSSFrameConstructor::ConstructScrollableBlock) };
        return &sScrollableBlockData[caption];
      }
    }

    // Handle various non-scrollable blocks.
    static const FrameConstructionData sNonScrollableBlockData[2][2] = {
      { FULL_CTOR_FCDATA(0,
                         &nsCSSFrameConstructor::ConstructNonScrollableBlock),
        FULL_CTOR_FCDATA(kCaptionCtorFlags,
                         &nsCSSFrameConstructor::ConstructNonScrollableBlock) },
      { FULL_CTOR_FCDATA(FCDATA_FORCED_NON_SCROLLABLE_BLOCK,
                         &nsCSSFrameConstructor::ConstructNonScrollableBlock),
        FULL_CTOR_FCDATA(FCDATA_FORCED_NON_SCROLLABLE_BLOCK | kCaptionCtorFlags,
                         &nsCSSFrameConstructor::ConstructNonScrollableBlock) }
    };
    return &sNonScrollableBlockData[suppressScrollFrame][caption];
  }

  // If this is for a <body> node and we've propagated the scroll-frame to the
  // viewport, we need to make sure not to add another layer of scrollbars, so
  // we use a different FCData struct without FCDATA_MAY_NEED_SCROLLFRAME.
  if (propagatedScrollToViewport && aDisplay->IsScrollableOverflow()) {
    if (aDisplay->mDisplay == StyleDisplay::Flex ||
        aDisplay->mDisplay == StyleDisplay::WebkitBox) {
      static const FrameConstructionData sNonScrollableFlexData =
        FCDATA_DECL(0, NS_NewFlexContainerFrame);
      return &sNonScrollableFlexData;
    }
    if (aDisplay->mDisplay == StyleDisplay::Grid) {
      static const FrameConstructionData sNonScrollableGridData =
        FCDATA_DECL(0, NS_NewGridContainerFrame);
      return &sNonScrollableGridData;
    }
  }

  // NOTE: Make sure to keep this up to date with the StyleDisplay definition!
  static const FrameConstructionDataByDisplay sDisplayData[] = {
    FCDATA_FOR_DISPLAY(StyleDisplay::None, UNREACHABLE_FCDATA()),
    FCDATA_FOR_DISPLAY(StyleDisplay::Block, UNREACHABLE_FCDATA()),
    // To keep the hash table small don't add inline frames (they're
    // typically things like FONT and B), because we can quickly
    // find them if we need to.
    // XXXbz the "quickly" part is a bald-faced lie!
    FCDATA_FOR_DISPLAY(StyleDisplay::Inline,
      FULL_CTOR_FCDATA(FCDATA_IS_INLINE | FCDATA_IS_LINE_PARTICIPANT,
                       &nsCSSFrameConstructor::ConstructInline)),
    FCDATA_FOR_DISPLAY(StyleDisplay::InlineBlock, UNREACHABLE_FCDATA()),
    FCDATA_FOR_DISPLAY(StyleDisplay::ListItem, UNREACHABLE_FCDATA()),
    FCDATA_FOR_DISPLAY(StyleDisplay::Table,
      FULL_CTOR_FCDATA(0, &nsCSSFrameConstructor::ConstructTable)),
    FCDATA_FOR_DISPLAY(StyleDisplay::InlineTable,
      FULL_CTOR_FCDATA(0, &nsCSSFrameConstructor::ConstructTable)),
    // NOTE: In the unlikely event that we add another table-part here that has
    // a desired-parent-type (& hence triggers table fixup), we'll need to also
    // update the flexbox chunk in nsStyleContext::ApplyStyleFixups().
    FCDATA_FOR_DISPLAY(StyleDisplay::TableRowGroup,
      FULL_CTOR_FCDATA(FCDATA_IS_TABLE_PART |
                       FCDATA_DESIRED_PARENT_TYPE_TO_BITS(eTypeTable),
                       &nsCSSFrameConstructor::ConstructTableRowOrRowGroup)),
    FCDATA_FOR_DISPLAY(StyleDisplay::TableColumn,
      FULL_CTOR_FCDATA(FCDATA_IS_TABLE_PART |
                       FCDATA_DESIRED_PARENT_TYPE_TO_BITS(eTypeColGroup),
                       &nsCSSFrameConstructor::ConstructTableCol)),
    FCDATA_FOR_DISPLAY(StyleDisplay::TableColumnGroup,
      FCDATA_DECL(FCDATA_IS_TABLE_PART | FCDATA_DISALLOW_OUT_OF_FLOW |
                  FCDATA_SKIP_ABSPOS_PUSH |
                  FCDATA_DESIRED_PARENT_TYPE_TO_BITS(eTypeTable),
                  NS_NewTableColGroupFrame)),
    FCDATA_FOR_DISPLAY(StyleDisplay::TableHeaderGroup,
      FULL_CTOR_FCDATA(FCDATA_IS_TABLE_PART |
                       FCDATA_DESIRED_PARENT_TYPE_TO_BITS(eTypeTable),
                       &nsCSSFrameConstructor::ConstructTableRowOrRowGroup)),
    FCDATA_FOR_DISPLAY(StyleDisplay::TableFooterGroup,
      FULL_CTOR_FCDATA(FCDATA_IS_TABLE_PART |
                       FCDATA_DESIRED_PARENT_TYPE_TO_BITS(eTypeTable),
                       &nsCSSFrameConstructor::ConstructTableRowOrRowGroup)),
    FCDATA_FOR_DISPLAY(StyleDisplay::TableRow,
      FULL_CTOR_FCDATA(FCDATA_IS_TABLE_PART |
                       FCDATA_DESIRED_PARENT_TYPE_TO_BITS(eTypeRowGroup),
                       &nsCSSFrameConstructor::ConstructTableRowOrRowGroup)),
    FCDATA_FOR_DISPLAY(StyleDisplay::TableCell,
      FULL_CTOR_FCDATA(FCDATA_IS_TABLE_PART |
                       FCDATA_DESIRED_PARENT_TYPE_TO_BITS(eTypeRow),
                       &nsCSSFrameConstructor::ConstructTableCell)),
    FCDATA_FOR_DISPLAY(StyleDisplay::TableCaption, UNREACHABLE_FCDATA()),
    FCDATA_FOR_DISPLAY(StyleDisplay::Flex,
      FCDATA_DECL(FCDATA_MAY_NEED_SCROLLFRAME, NS_NewFlexContainerFrame)),
    FCDATA_FOR_DISPLAY(StyleDisplay::InlineFlex,
      FCDATA_DECL(FCDATA_MAY_NEED_SCROLLFRAME, NS_NewFlexContainerFrame)),
    FCDATA_FOR_DISPLAY(StyleDisplay::Grid,
      FCDATA_DECL(FCDATA_MAY_NEED_SCROLLFRAME, NS_NewGridContainerFrame)),
    FCDATA_FOR_DISPLAY(StyleDisplay::InlineGrid,
      FCDATA_DECL(FCDATA_MAY_NEED_SCROLLFRAME, NS_NewGridContainerFrame)),
    FCDATA_FOR_DISPLAY(StyleDisplay::Ruby,
      FCDATA_DECL(FCDATA_IS_LINE_PARTICIPANT, NS_NewRubyFrame)),
    FCDATA_FOR_DISPLAY(StyleDisplay::RubyBase,
      FCDATA_DECL(FCDATA_IS_LINE_PARTICIPANT |
                  FCDATA_DESIRED_PARENT_TYPE_TO_BITS(eTypeRubyBaseContainer),
                  NS_NewRubyBaseFrame)),
    FCDATA_FOR_DISPLAY(StyleDisplay::RubyBaseContainer,
      FCDATA_DECL(FCDATA_IS_LINE_PARTICIPANT |
                  FCDATA_DESIRED_PARENT_TYPE_TO_BITS(eTypeRuby),
                  NS_NewRubyBaseContainerFrame)),
    FCDATA_FOR_DISPLAY(StyleDisplay::RubyText,
      FCDATA_DECL(FCDATA_IS_LINE_PARTICIPANT |
                  FCDATA_DESIRED_PARENT_TYPE_TO_BITS(eTypeRubyTextContainer),
                  NS_NewRubyTextFrame)),
    FCDATA_FOR_DISPLAY(StyleDisplay::RubyTextContainer,
      FCDATA_DECL(FCDATA_DESIRED_PARENT_TYPE_TO_BITS(eTypeRuby),
                  NS_NewRubyTextContainerFrame)),
    FCDATA_FOR_DISPLAY(StyleDisplay::Contents,
      FULL_CTOR_FCDATA(FCDATA_IS_CONTENTS, nullptr/*never called*/)),
    FCDATA_FOR_DISPLAY(StyleDisplay::WebkitBox,
      FCDATA_DECL(FCDATA_MAY_NEED_SCROLLFRAME, NS_NewFlexContainerFrame)),
    FCDATA_FOR_DISPLAY(StyleDisplay::WebkitInlineBox,
      FCDATA_DECL(FCDATA_MAY_NEED_SCROLLFRAME, NS_NewFlexContainerFrame)),
  };
  static_assert(ArrayLength(sDisplayData) == size_t(StyleDisplay::WebkitInlineBox) + 1,
                "Be sure to update sDisplayData if you touch StyleDisplay");

  MOZ_ASSERT(size_t(aDisplay->mDisplay) < ArrayLength(sDisplayData),
             "XUL display data should have already been handled");

  // See the mDisplay fixup code in nsRuleNode::ComputeDisplayData.
  MOZ_ASSERT(aDisplay->mDisplay != StyleDisplay::Contents ||
             !aElement->IsRootOfNativeAnonymousSubtree(),
             "display:contents on anonymous content is unsupported");

  const FrameConstructionDataByDisplay& data =
    sDisplayData[size_t(aDisplay->mDisplay)];

  MOZ_ASSERT(data.mDisplay == aDisplay->mDisplay,
             "Someone messed up the order in the display values");

  return &data.mData;
}

nsIFrame*
nsCSSFrameConstructor::ConstructScrollableBlock(nsFrameConstructorState& aState,
                                                FrameConstructionItem&   aItem,
                                                nsContainerFrame*        aParentFrame,
                                                const nsStyleDisplay*    aDisplay,
                                                nsFrameItems&            aFrameItems)
{
  return ConstructScrollableBlockWithConstructor(aState, aItem, aParentFrame,
                                                 aDisplay, aFrameItems,
                                                 NS_NewBlockFormattingContext);
}

nsIFrame*
nsCSSFrameConstructor::ConstructScrollableBlockWithConstructor(
  nsFrameConstructorState& aState,
  FrameConstructionItem& aItem,
  nsContainerFrame* aParentFrame,
  const nsStyleDisplay* aDisplay,
  nsFrameItems& aFrameItems,
  BlockFrameCreationFunc aConstructor)
{
  nsIContent* const content = aItem.mContent;
  nsStyleContext* const styleContext = aItem.mStyleContext;

  nsContainerFrame* newFrame = nullptr;
  RefPtr<nsStyleContext> scrolledContentStyle
    = BeginBuildingScrollFrame(aState, content, styleContext,
                               aState.GetGeometricParent(aDisplay, aParentFrame),
                               nsCSSAnonBoxes::scrolledContent,
                               false, newFrame);

  // Create our block frame
  // pass a temporary stylecontext, the correct one will be set later
  nsContainerFrame* scrolledFrame = aConstructor(mPresShell, styleContext);

  // Make sure to AddChild before we call ConstructBlock so that we
  // end up before our descendants in fixed-pos lists as needed.
  aState.AddChild(newFrame, aFrameItems, content, styleContext, aParentFrame);

  nsFrameItems blockItem;
  ConstructBlock(aState, content, newFrame, newFrame, scrolledContentStyle,
                 &scrolledFrame, blockItem,
                 aDisplay->IsAbsPosContainingBlock(newFrame) ? newFrame : nullptr,
                 aItem.mPendingBinding);

  MOZ_ASSERT(blockItem.OnlyChild() == scrolledFrame,
             "Scrollframe's frameItems should be exactly the scrolled frame!");
  FinishBuildingScrollFrame(newFrame, scrolledFrame);

  return newFrame;
}

nsIFrame*
nsCSSFrameConstructor::ConstructNonScrollableBlock(nsFrameConstructorState& aState,
                                                   FrameConstructionItem&   aItem,
                                                   nsContainerFrame*        aParentFrame,
                                                   const nsStyleDisplay*    aDisplay,
                                                   nsFrameItems&            aFrameItems)
{
  return ConstructNonScrollableBlockWithConstructor(aState, aItem, aParentFrame,
                                                    aDisplay, aFrameItems,
                                                    NS_NewBlockFrame);
}

nsIFrame*
nsCSSFrameConstructor::ConstructNonScrollableBlockWithConstructor(
  nsFrameConstructorState& aState,
  FrameConstructionItem& aItem,
  nsContainerFrame* aParentFrame,
  const nsStyleDisplay* aDisplay,
  nsFrameItems& aFrameItems,
  BlockFrameCreationFunc aConstructor)
{
  nsStyleContext* const styleContext = aItem.mStyleContext;

  // We want a block formatting context root in paginated contexts for
  // every block that would be scrollable in a non-paginated context.
  // We mark our blocks with a bit here if this condition is true, so
  // we can check it later in nsFrame::ApplyPaginatedOverflowClipping.
  bool clipPaginatedOverflow =
    (aItem.mFCData->mBits & FCDATA_FORCED_NON_SCROLLABLE_BLOCK) != 0;
  nsFrameState flags = nsFrameState(0);
  if ((aDisplay->IsAbsolutelyPositionedStyle() ||
       aDisplay->IsFloatingStyle() ||
       StyleDisplay::InlineBlock == aDisplay->mDisplay ||
       clipPaginatedOverflow) &&
      !aParentFrame->IsSVGText()) {
    flags = NS_BLOCK_FLOAT_MGR | NS_BLOCK_MARGIN_ROOT;
    if (clipPaginatedOverflow) {
      flags |= NS_BLOCK_CLIP_PAGINATED_OVERFLOW;
    }
  }

  nsContainerFrame* newFrame = aConstructor(mPresShell, styleContext);
  newFrame->AddStateBits(flags);
  ConstructBlock(aState, aItem.mContent,
                 aState.GetGeometricParent(aDisplay, aParentFrame),
                 aParentFrame, styleContext, &newFrame,
                 aFrameItems,
                 aDisplay->IsAbsPosContainingBlock(newFrame) ? newFrame : nullptr,
                 aItem.mPendingBinding);
  return newFrame;
}


void
nsCSSFrameConstructor::InitAndRestoreFrame(const nsFrameConstructorState& aState,
                                           nsIContent*              aContent,
                                           nsContainerFrame*        aParentFrame,
                                           nsIFrame*                aNewFrame,
                                           bool                     aAllowCounters)
{
  NS_PRECONDITION(mUpdateCount != 0,
                  "Should be in an update while creating frames");

  MOZ_ASSERT(aNewFrame, "Null frame cannot be initialized");

  // Initialize the frame
  aNewFrame->Init(aContent, aParentFrame, nullptr);
  aNewFrame->AddStateBits(aState.mAdditionalStateBits);

  if (aState.mFrameState) {
    // Restore frame state for just the newly created frame.
    RestoreFrameStateFor(aNewFrame, aState.mFrameState);
  }

  if (aAllowCounters &&
      mCounterManager.AddCounterResetsAndIncrements(aNewFrame)) {
    CountersDirty();
  }
}

already_AddRefed<nsStyleContext>
nsCSSFrameConstructor::ResolveStyleContext(nsIFrame*         aParentFrame,
                                           nsIContent*       aContainer,
                                           nsIContent*       aChild,
                                           nsFrameConstructorState* aState)
{
  MOZ_ASSERT(aContainer, "Must have parent here");
  // XXX uncomment when bug 1089223 is fixed:
  // MOZ_ASSERT(aContainer == aChild->GetFlattenedTreeParent());
  nsStyleContext* parentStyleContext = GetDisplayContentsStyleFor(aContainer);
  if (MOZ_LIKELY(!parentStyleContext)) {
    aParentFrame = nsFrame::CorrectStyleParentFrame(aParentFrame, nullptr);
    if (aParentFrame) {
      MOZ_ASSERT(aParentFrame->GetContent() == aContainer);
      // Resolve the style context based on the content object and the parent
      // style context
      parentStyleContext = aParentFrame->StyleContext();
    } else {
      // Perhaps aParentFrame is a canvasFrame and we're replicating
      // fixed-pos frames.
      // XXX should we create a way to tell ConstructFrame which style
      // context to use, and pass it the style context for the
      // previous page's fixed-pos frame?
    }
  }

  return ResolveStyleContext(parentStyleContext, aChild, aState);
}

already_AddRefed<nsStyleContext>
nsCSSFrameConstructor::ResolveStyleContext(nsIFrame*          aParentFrame,
                                           nsIContent*              aChild,
                                           nsFrameConstructorState* aState)
{
  return ResolveStyleContext(aParentFrame, aChild->GetFlattenedTreeParent(), aChild, aState);
}

already_AddRefed<nsStyleContext>
nsCSSFrameConstructor::ResolveStyleContext(const InsertionPoint&    aInsertion,
                                           nsIContent*              aChild,
                                           nsFrameConstructorState* aState)
{
  return ResolveStyleContext(aInsertion.mParentFrame, aInsertion.mContainer,
                             aChild, aState);
}

already_AddRefed<nsStyleContext>
nsCSSFrameConstructor::ResolveStyleContext(nsStyleContext* aParentStyleContext,
                                           nsIContent* aContent,
                                           nsFrameConstructorState* aState)
{
  StyleSetHandle styleSet = mPresShell->StyleSet();
  aContent->OwnerDoc()->FlushPendingLinkUpdates();

  RefPtr<nsStyleContext> result;
  if (aContent->IsElement()) {
    if (aState) {
      result = styleSet->ResolveStyleFor(aContent->AsElement(),
                                         aParentStyleContext,
                                         ConsumeStyleBehavior::Consume,
                                         LazyComputeBehavior::Assert,
                                         aState->mTreeMatchContext);
    } else {
      result = styleSet->ResolveStyleFor(aContent->AsElement(),
                                         aParentStyleContext,
                                         ConsumeStyleBehavior::Consume,
                                         LazyComputeBehavior::Assert);
    }
  } else {
    NS_ASSERTION(aContent->IsNodeOfType(nsINode::eTEXT),
                 "shouldn't waste time creating style contexts for "
                 "comments and processing instructions");
    result = styleSet->ResolveStyleForText(aContent, aParentStyleContext);
  }

  // ServoRestyleManager does not handle transitions yet, and when it does
  // it probably won't need to track reframed style contexts to start
  // transitions correctly.
  if (mozilla::RestyleManager* geckoRM = RestyleManager()->GetAsGecko()) {
    RestyleManager::ReframingStyleContexts* rsc =
      geckoRM->GetReframingStyleContexts();
    if (rsc) {
      nsStyleContext* oldStyleContext =
        rsc->Get(aContent, CSSPseudoElementType::NotPseudo);
      nsPresContext* presContext = mPresShell->GetPresContext();
      if (oldStyleContext) {
        RestyleManager::TryInitiatingTransition(presContext, aContent,
                                                oldStyleContext, &result);
      } else if (aContent->IsElement()) {
        presContext->TransitionManager()->
          PruneCompletedTransitions(aContent->AsElement(),
            CSSPseudoElementType::NotPseudo, result);
      }
    }
  }

  return result.forget();
}

// MathML Mod - RBS
void
nsCSSFrameConstructor::FlushAccumulatedBlock(nsFrameConstructorState& aState,
                                             nsIContent* aContent,
                                             nsContainerFrame* aParentFrame,
                                             nsFrameItems& aBlockItems,
                                             nsFrameItems& aNewItems)
{
  if (aBlockItems.IsEmpty()) {
    // Nothing to do
    return;
  }

  nsIAtom* anonPseudo = nsCSSAnonBoxes::mozMathMLAnonymousBlock;

  nsStyleContext* parentContext =
    nsFrame::CorrectStyleParentFrame(aParentFrame,
                                     anonPseudo)->StyleContext();
  StyleSetHandle styleSet = mPresShell->StyleSet();
  RefPtr<nsStyleContext> blockContext;
  blockContext = styleSet->
    ResolveAnonymousBoxStyle(anonPseudo, parentContext);


  // then, create a block frame that will wrap the child frames. Make it a
  // MathML frame so that Get(Absolute/Float)ContainingBlockFor know that this
  // is not a suitable block.
  nsContainerFrame* blockFrame =
    NS_NewMathMLmathBlockFrame(mPresShell, blockContext);
  blockFrame->AddStateBits(NS_BLOCK_FLOAT_MGR | NS_BLOCK_MARGIN_ROOT);

  InitAndRestoreFrame(aState, aContent, aParentFrame, blockFrame);
  ReparentFrames(this, blockFrame, aBlockItems);
  // abs-pos and floats are disabled in MathML children so we don't have to
  // worry about messing up those.
  blockFrame->SetInitialChildList(kPrincipalList, aBlockItems);
  NS_ASSERTION(aBlockItems.IsEmpty(), "What happened?");
  aBlockItems.Clear();
  aNewItems.AddChild(blockFrame);
}

// Only <math> elements can be floated or positioned.  All other MathML
// should be in-flow.
#define SIMPLE_MATHML_CREATE(_tag, _func)                               \
  { &nsGkAtoms::_tag,                                                   \
      FCDATA_DECL(FCDATA_DISALLOW_OUT_OF_FLOW |                         \
                  FCDATA_FORCE_NULL_ABSPOS_CONTAINER |                  \
                  FCDATA_WRAP_KIDS_IN_BLOCKS, _func) }

/* static */
const nsCSSFrameConstructor::FrameConstructionData*
nsCSSFrameConstructor::FindMathMLData(Element* aElement,
                                      nsIAtom* aTag,
                                      int32_t aNameSpaceID,
                                      nsStyleContext* aStyleContext)
{
  // Make sure that we remain confined in the MathML world
  if (aNameSpaceID != kNameSpaceID_MathML)
    return nullptr;

  // Handle <math> specially, because it sometimes produces inlines
  if (aTag == nsGkAtoms::math) {
    // This needs to match the test in EnsureBlockDisplay in
    // nsRuleNode.cpp.  Though the behavior here for the display:table
    // case is pretty weird...
    if (aStyleContext->StyleDisplay()->IsBlockOutsideStyle()) {
      static const FrameConstructionData sBlockMathData =
        FCDATA_DECL(FCDATA_FORCE_NULL_ABSPOS_CONTAINER |
                    FCDATA_WRAP_KIDS_IN_BLOCKS,
                    NS_NewMathMLmathBlockFrame);
      return &sBlockMathData;
    }

    static const FrameConstructionData sInlineMathData =
      FCDATA_DECL(FCDATA_FORCE_NULL_ABSPOS_CONTAINER |
                  FCDATA_IS_LINE_PARTICIPANT |
                  FCDATA_WRAP_KIDS_IN_BLOCKS,
                  NS_NewMathMLmathInlineFrame);
    return &sInlineMathData;
  }


  static const FrameConstructionDataByTag sMathMLData[] = {
    SIMPLE_MATHML_CREATE(annotation_, NS_NewMathMLTokenFrame),
    SIMPLE_MATHML_CREATE(annotation_xml_, NS_NewMathMLmrowFrame),
    SIMPLE_MATHML_CREATE(mi_, NS_NewMathMLTokenFrame),
    SIMPLE_MATHML_CREATE(mn_, NS_NewMathMLTokenFrame),
    SIMPLE_MATHML_CREATE(ms_, NS_NewMathMLTokenFrame),
    SIMPLE_MATHML_CREATE(mtext_, NS_NewMathMLTokenFrame),
    SIMPLE_MATHML_CREATE(mo_, NS_NewMathMLmoFrame),
    SIMPLE_MATHML_CREATE(mfrac_, NS_NewMathMLmfracFrame),
    SIMPLE_MATHML_CREATE(msup_, NS_NewMathMLmmultiscriptsFrame),
    SIMPLE_MATHML_CREATE(msub_, NS_NewMathMLmmultiscriptsFrame),
    SIMPLE_MATHML_CREATE(msubsup_, NS_NewMathMLmmultiscriptsFrame),
    SIMPLE_MATHML_CREATE(munder_, NS_NewMathMLmunderoverFrame),
    SIMPLE_MATHML_CREATE(mover_, NS_NewMathMLmunderoverFrame),
    SIMPLE_MATHML_CREATE(munderover_, NS_NewMathMLmunderoverFrame),
    SIMPLE_MATHML_CREATE(mphantom_, NS_NewMathMLmrowFrame),
    SIMPLE_MATHML_CREATE(mpadded_, NS_NewMathMLmpaddedFrame),
    SIMPLE_MATHML_CREATE(mspace_, NS_NewMathMLmspaceFrame),
    SIMPLE_MATHML_CREATE(none, NS_NewMathMLmspaceFrame),
    SIMPLE_MATHML_CREATE(mprescripts_, NS_NewMathMLmspaceFrame),
    SIMPLE_MATHML_CREATE(mfenced_, NS_NewMathMLmfencedFrame),
    SIMPLE_MATHML_CREATE(mmultiscripts_, NS_NewMathMLmmultiscriptsFrame),
    SIMPLE_MATHML_CREATE(mstyle_, NS_NewMathMLmrowFrame),
    SIMPLE_MATHML_CREATE(msqrt_, NS_NewMathMLmsqrtFrame),
    SIMPLE_MATHML_CREATE(mroot_, NS_NewMathMLmrootFrame),
    SIMPLE_MATHML_CREATE(maction_, NS_NewMathMLmactionFrame),
    SIMPLE_MATHML_CREATE(mrow_, NS_NewMathMLmrowFrame),
    SIMPLE_MATHML_CREATE(merror_, NS_NewMathMLmrowFrame),
    SIMPLE_MATHML_CREATE(menclose_, NS_NewMathMLmencloseFrame),
    SIMPLE_MATHML_CREATE(semantics_, NS_NewMathMLsemanticsFrame)
  };

  return FindDataByTag(aTag, aElement, aStyleContext, sMathMLData,
                       ArrayLength(sMathMLData));
}


nsContainerFrame*
nsCSSFrameConstructor::ConstructFrameWithAnonymousChild(
                                   nsFrameConstructorState& aState,
                                   FrameConstructionItem&   aItem,
                                   nsContainerFrame*        aParentFrame,
                                   nsFrameItems&            aFrameItems,
                                   ContainerFrameCreationFunc aConstructor,
                                   ContainerFrameCreationFunc aInnerConstructor,
                                   nsICSSAnonBoxPseudo*     aInnerPseudo,
                                   bool                     aCandidateRootFrame)
{
  nsIContent* const content = aItem.mContent;
  nsStyleContext* const styleContext = aItem.mStyleContext;

  // Create the outer frame:
  nsContainerFrame* newFrame = aConstructor(mPresShell, styleContext);

  InitAndRestoreFrame(aState, content,
                      aCandidateRootFrame ?
                        aState.GetGeometricParent(styleContext->StyleDisplay(),
                                                  aParentFrame) :
                        aParentFrame,
                      newFrame);

  // Create the pseudo SC for the anonymous wrapper child as a child of the SC:
  RefPtr<nsStyleContext> scForAnon;
  scForAnon = mPresShell->StyleSet()->
    ResolveAnonymousBoxStyle(aInnerPseudo, styleContext);

  // Create the anonymous inner wrapper frame
  nsContainerFrame* innerFrame = aInnerConstructor(mPresShell, scForAnon);

  InitAndRestoreFrame(aState, content, newFrame, innerFrame);

  // Put the newly created frames into the right child list
  SetInitialSingleChild(newFrame, innerFrame);

  aState.AddChild(newFrame, aFrameItems, content, styleContext, aParentFrame,
                  aCandidateRootFrame, aCandidateRootFrame);

  if (!mRootElementFrame && aCandidateRootFrame) {
    // The frame we're constructing will be the root element frame.
    // Set mRootElementFrame before processing children.
    mRootElementFrame = newFrame;
  }

  nsFrameItems childItems;

  // Process children
  NS_ASSERTION(aItem.mAnonChildren.IsEmpty(),
               "nsIAnonymousContentCreator::CreateAnonymousContent should not "
               "be implemented for frames for which we explicitly create an "
               "anonymous child to wrap its child frames");
  if (aItem.mFCData->mBits & FCDATA_USE_CHILD_ITEMS) {
    ConstructFramesFromItemList(aState, aItem.mChildItems,
                                innerFrame, childItems);
  } else {
    ProcessChildren(aState, content, styleContext, innerFrame,
                    true, childItems, false, aItem.mPendingBinding);
  }

  // Set the inner wrapper frame's initial primary list
  innerFrame->SetInitialChildList(kPrincipalList, childItems);

  return newFrame;
}

nsIFrame*
nsCSSFrameConstructor::ConstructOuterSVG(nsFrameConstructorState& aState,
                                         FrameConstructionItem&   aItem,
                                         nsContainerFrame*        aParentFrame,
                                         const nsStyleDisplay*    aDisplay,
                                         nsFrameItems&            aFrameItems)
{
  return ConstructFrameWithAnonymousChild(
      aState, aItem, aParentFrame, aFrameItems,
      NS_NewSVGOuterSVGFrame, NS_NewSVGOuterSVGAnonChildFrame,
      nsCSSAnonBoxes::mozSVGOuterSVGAnonChild, true);
}

nsIFrame*
nsCSSFrameConstructor::ConstructMarker(nsFrameConstructorState& aState,
                                       FrameConstructionItem&   aItem,
                                       nsContainerFrame*        aParentFrame,
                                       const nsStyleDisplay*    aDisplay,
                                       nsFrameItems&            aFrameItems)
{
  return ConstructFrameWithAnonymousChild(
      aState, aItem, aParentFrame, aFrameItems,
      NS_NewSVGMarkerFrame, NS_NewSVGMarkerAnonChildFrame,
      nsCSSAnonBoxes::mozSVGMarkerAnonChild, false);
}

// Only outer <svg> elements can be floated or positioned.  All other SVG
// should be in-flow.
#define SIMPLE_SVG_FCDATA(_func)                                        \
  FCDATA_DECL(FCDATA_DISALLOW_OUT_OF_FLOW |                             \
              FCDATA_SKIP_ABSPOS_PUSH |                                 \
              FCDATA_DISALLOW_GENERATED_CONTENT,  _func)
#define SIMPLE_SVG_CREATE(_tag, _func)            \
  { &nsGkAtoms::_tag, SIMPLE_SVG_FCDATA(_func) }

static bool
IsFilterPrimitiveChildTag(const nsIAtom* aTag)
{
  return aTag == nsGkAtoms::feDistantLight ||
         aTag == nsGkAtoms::fePointLight ||
         aTag == nsGkAtoms::feSpotLight ||
         aTag == nsGkAtoms::feFuncR ||
         aTag == nsGkAtoms::feFuncG ||
         aTag == nsGkAtoms::feFuncB ||
         aTag == nsGkAtoms::feFuncA ||
         aTag == nsGkAtoms::feMergeNode;
}

/* static */
const nsCSSFrameConstructor::FrameConstructionData*
nsCSSFrameConstructor::FindSVGData(Element* aElement,
                                   nsIAtom* aTag,
                                   int32_t aNameSpaceID,
                                   nsIFrame* aParentFrame,
                                   bool aIsWithinSVGText,
                                   bool aAllowsTextPathChild,
                                   nsStyleContext* aStyleContext)
{
  if (aNameSpaceID != kNameSpaceID_SVG) {
    return nullptr;
  }

  static const FrameConstructionData sSuppressData = SUPPRESS_FCDATA();
  static const FrameConstructionData sContainerData =
    SIMPLE_SVG_FCDATA(NS_NewSVGContainerFrame);

  bool parentIsSVG = aIsWithinSVGText;
  nsIContent* parentContent =
    aParentFrame ? aParentFrame->GetContent() : nullptr;
  // XXXbz should this really be based on the XBL-resolved tag of the parent
  // frame's content?  Should it not be based on the type of the parent frame
  // (e.g. whether it's an SVG frame)?
  if (parentContent) {
    int32_t parentNSID;
    nsIAtom* parentTag =
      parentContent->OwnerDoc()->BindingManager()->
        ResolveTag(parentContent, &parentNSID);

    // It's not clear whether the SVG spec intends to allow any SVG
    // content within svg:foreignObject at all (SVG 1.1, section
    // 23.2), but if it does, it better be svg:svg.  So given that
    // we're allowing it, treat it as a non-SVG parent.
    parentIsSVG = parentNSID == kNameSpaceID_SVG &&
                  parentTag != nsGkAtoms::foreignObject;
  }

  if ((aTag != nsGkAtoms::svg && !parentIsSVG) ||
      (aTag == nsGkAtoms::desc || aTag == nsGkAtoms::title ||
       aTag == nsGkAtoms::metadata)) {
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
    // We don't currently handle any UI for desc/title/metadata
    return &sSuppressData;
  }

  // We don't need frames for animation elements
  if (aElement->IsNodeOfType(nsINode::eANIMATION)) {
    return &sSuppressData;
  }

  if (aTag == nsGkAtoms::svg && !parentIsSVG) {
    // We need outer <svg> elements to have an nsSVGOuterSVGFrame regardless
    // of whether they fail conditional processing attributes, since various
    // SVG frames assume that one exists.  We handle the non-rendering
    // of failing outer <svg> element contents like <switch> statements,
    // and do the PassesConditionalProcessingTests call in
    // nsSVGOuterSVGFrame::Init.
    static const FrameConstructionData sOuterSVGData =
      FULL_CTOR_FCDATA(0, &nsCSSFrameConstructor::ConstructOuterSVG);
    return &sOuterSVGData;
  }

  if (aTag == nsGkAtoms::marker) {
    static const FrameConstructionData sMarkerSVGData =
      FULL_CTOR_FCDATA(0, &nsCSSFrameConstructor::ConstructMarker);
    return &sMarkerSVGData;
  }

  nsCOMPtr<SVGTests> tests(do_QueryInterface(aElement));
  if (tests && !tests->PassesConditionalProcessingTests()) {
    // Elements with failing conditional processing attributes never get
    // rendered.  Note that this is not where we select which frame in a
    // <switch> to render!  That happens in nsSVGSwitchFrame::PaintSVG.
    if (aIsWithinSVGText) {
      // SVGTextFrame doesn't handle conditional processing attributes,
      // so don't create frames for descendants of <text> with failing
      // attributes.  We need frames not to be created so that text layout
      // is correct.
      return &sSuppressData;
    }
    // If we're not inside <text>, create an nsSVGContainerFrame (which is a
    // frame that doesn't render) so that paint servers can still be referenced,
    // even if they live inside an element with failing conditional processing
    // attributes.
    return &sContainerData;
  }

  // Ensure that a stop frame is a child of a gradient and that gradients
  // can only have stop children.
  bool parentIsGradient = aParentFrame &&
    (aParentFrame->GetType() == nsGkAtoms::svgLinearGradientFrame ||
     aParentFrame->GetType() == nsGkAtoms::svgRadialGradientFrame);
  bool stop = (aTag == nsGkAtoms::stop);
  if ((parentIsGradient && !stop) ||
      (!parentIsGradient && stop)) {
    return &sSuppressData;
  }

  // Prevent bad frame types being children of filters or parents of filter
  // primitives.  If aParentFrame is null, we know that the frame that will
  // be created will be an nsInlineFrame, so it can never be a filter.
  bool parentIsFilter = aParentFrame &&
    aParentFrame->GetType() == nsGkAtoms::svgFilterFrame;
  bool filterPrimitive = aElement->IsNodeOfType(nsINode::eFILTER);
  if ((parentIsFilter && !filterPrimitive) ||
      (!parentIsFilter && filterPrimitive)) {
    return &sSuppressData;
  }

  // Prevent bad frame types being children of filter primitives or parents of
  // filter primitive children.  If aParentFrame is null, we know that the frame
  // that will be created will be an nsInlineFrame, so it can never be a filter
  // primitive.
  bool parentIsFEContainerFrame = aParentFrame &&
    aParentFrame->GetType() == nsGkAtoms::svgFEContainerFrame;
  if ((parentIsFEContainerFrame && !IsFilterPrimitiveChildTag(aTag)) ||
      (!parentIsFEContainerFrame && IsFilterPrimitiveChildTag(aTag))) {
    return &sSuppressData;
  }

  // Special cases for text/tspan/textPath, because the kind of frame
  // they get depends on the parent frame.  We ignore 'a' elements when
  // determining the parent, however.
  if (aIsWithinSVGText) {
    // If aIsWithinSVGText is true, then we know that the "SVG text uses
    // CSS frames" pref was true when this SVG fragment was first constructed.

    // We don't use ConstructInline because we want different behavior
    // for generated content.
    static const FrameConstructionData sTSpanData =
      FCDATA_DECL(FCDATA_DISALLOW_OUT_OF_FLOW |
                  FCDATA_SKIP_ABSPOS_PUSH |
                  FCDATA_DISALLOW_GENERATED_CONTENT |
                  FCDATA_IS_LINE_PARTICIPANT |
                  FCDATA_IS_INLINE |
                  FCDATA_USE_CHILD_ITEMS,
                  NS_NewInlineFrame);
    if (aTag == nsGkAtoms::textPath) {
      if (aAllowsTextPathChild) {
        return &sTSpanData;
      }
    } else if (aTag == nsGkAtoms::tspan ||
               aTag == nsGkAtoms::a) {
      return &sTSpanData;
    }
    return &sSuppressData;
  } else if (aTag == nsGkAtoms::text) {
    static const FrameConstructionData sTextData =
      FCDATA_WITH_WRAPPING_BLOCK(FCDATA_DISALLOW_OUT_OF_FLOW |
                                 FCDATA_ALLOW_BLOCK_STYLES,
                                 NS_NewSVGTextFrame,
                                 nsCSSAnonBoxes::mozSVGText);
    return &sTextData;
  } else if (aTag == nsGkAtoms::tspan ||
             aTag == nsGkAtoms::textPath) {
    return &sSuppressData;
  }

  static const FrameConstructionDataByTag sSVGData[] = {
    SIMPLE_SVG_CREATE(svg, NS_NewSVGInnerSVGFrame),
    SIMPLE_SVG_CREATE(g, NS_NewSVGGFrame),
    SIMPLE_SVG_CREATE(svgSwitch, NS_NewSVGSwitchFrame),
    SIMPLE_SVG_CREATE(polygon, NS_NewSVGPathGeometryFrame),
    SIMPLE_SVG_CREATE(polyline, NS_NewSVGPathGeometryFrame),
    SIMPLE_SVG_CREATE(circle, NS_NewSVGPathGeometryFrame),
    SIMPLE_SVG_CREATE(ellipse, NS_NewSVGPathGeometryFrame),
    SIMPLE_SVG_CREATE(line, NS_NewSVGPathGeometryFrame),
    SIMPLE_SVG_CREATE(rect, NS_NewSVGPathGeometryFrame),
    SIMPLE_SVG_CREATE(path, NS_NewSVGPathGeometryFrame),
    SIMPLE_SVG_CREATE(defs, NS_NewSVGContainerFrame),
    SIMPLE_SVG_CREATE(generic_, NS_NewSVGGenericContainerFrame),
    { &nsGkAtoms::foreignObject,
      FCDATA_WITH_WRAPPING_BLOCK(FCDATA_DISALLOW_OUT_OF_FLOW,
                                 NS_NewSVGForeignObjectFrame,
                                 nsCSSAnonBoxes::mozSVGForeignContent) },
    SIMPLE_SVG_CREATE(a, NS_NewSVGAFrame),
    SIMPLE_SVG_CREATE(linearGradient, NS_NewSVGLinearGradientFrame),
    SIMPLE_SVG_CREATE(radialGradient, NS_NewSVGRadialGradientFrame),
    SIMPLE_SVG_CREATE(stop, NS_NewSVGStopFrame),
    SIMPLE_SVG_CREATE(use, NS_NewSVGUseFrame),
    SIMPLE_SVG_CREATE(view, NS_NewSVGViewFrame),
    SIMPLE_SVG_CREATE(image, NS_NewSVGImageFrame),
    SIMPLE_SVG_CREATE(clipPath, NS_NewSVGClipPathFrame),
    SIMPLE_SVG_CREATE(filter, NS_NewSVGFilterFrame),
    SIMPLE_SVG_CREATE(pattern, NS_NewSVGPatternFrame),
    SIMPLE_SVG_CREATE(mask, NS_NewSVGMaskFrame),
    SIMPLE_SVG_CREATE(feDistantLight, NS_NewSVGFEUnstyledLeafFrame),
    SIMPLE_SVG_CREATE(fePointLight, NS_NewSVGFEUnstyledLeafFrame),
    SIMPLE_SVG_CREATE(feSpotLight, NS_NewSVGFEUnstyledLeafFrame),
    SIMPLE_SVG_CREATE(feBlend, NS_NewSVGFELeafFrame),
    SIMPLE_SVG_CREATE(feColorMatrix, NS_NewSVGFELeafFrame),
    SIMPLE_SVG_CREATE(feFuncR, NS_NewSVGFEUnstyledLeafFrame),
    SIMPLE_SVG_CREATE(feFuncG, NS_NewSVGFEUnstyledLeafFrame),
    SIMPLE_SVG_CREATE(feFuncB, NS_NewSVGFEUnstyledLeafFrame),
    SIMPLE_SVG_CREATE(feFuncA, NS_NewSVGFEUnstyledLeafFrame),
    SIMPLE_SVG_CREATE(feComposite, NS_NewSVGFELeafFrame),
    SIMPLE_SVG_CREATE(feComponentTransfer, NS_NewSVGFEContainerFrame),
    SIMPLE_SVG_CREATE(feConvolveMatrix, NS_NewSVGFELeafFrame),
    SIMPLE_SVG_CREATE(feDiffuseLighting, NS_NewSVGFEContainerFrame),
    SIMPLE_SVG_CREATE(feDisplacementMap, NS_NewSVGFELeafFrame),
    SIMPLE_SVG_CREATE(feDropShadow, NS_NewSVGFELeafFrame),
    SIMPLE_SVG_CREATE(feFlood, NS_NewSVGFELeafFrame),
    SIMPLE_SVG_CREATE(feGaussianBlur, NS_NewSVGFELeafFrame),
    SIMPLE_SVG_CREATE(feImage, NS_NewSVGFEImageFrame),
    SIMPLE_SVG_CREATE(feMerge, NS_NewSVGFEContainerFrame),
    SIMPLE_SVG_CREATE(feMergeNode, NS_NewSVGFEUnstyledLeafFrame),
    SIMPLE_SVG_CREATE(feMorphology, NS_NewSVGFELeafFrame),
    SIMPLE_SVG_CREATE(feOffset, NS_NewSVGFELeafFrame),
    SIMPLE_SVG_CREATE(feSpecularLighting, NS_NewSVGFEContainerFrame),
    SIMPLE_SVG_CREATE(feTile, NS_NewSVGFELeafFrame),
    SIMPLE_SVG_CREATE(feTurbulence, NS_NewSVGFELeafFrame)
  };

  const FrameConstructionData* data =
    FindDataByTag(aTag, aElement, aStyleContext, sSVGData,
                  ArrayLength(sSVGData));

  if (!data) {
    data = &sContainerData;
  }

  return data;
}

void
nsCSSFrameConstructor::AddPageBreakItem(nsIContent* aContent,
                                        nsStyleContext* aMainStyleContext,
                                        FrameConstructionItemList& aItems)
{
  // Use the same parent style context that |aMainStyleContext| has, since
  // that's easier to re-resolve and it doesn't matter in practice.
  // (Getting different parents can result in framechange hints, e.g.,
  // for user-modify.)
  RefPtr<nsStyleContext> pseudoStyle =
    mPresShell->StyleSet()->
      ResolveAnonymousBoxStyle(nsCSSAnonBoxes::pageBreak,
                               aMainStyleContext->GetParent());

  MOZ_ASSERT(pseudoStyle->StyleDisplay()->mDisplay == StyleDisplay::Block,
             "Unexpected display");

  static const FrameConstructionData sPageBreakData =
    FCDATA_DECL(FCDATA_SKIP_FRAMESET, NS_NewPageBreakFrame);

  // Lie about the tag and namespace so we don't trigger anything
  // interesting during frame construction.
  aItems.AppendItem(&sPageBreakData, aContent, nsCSSAnonBoxes::pageBreak,
                    kNameSpaceID_None, nullptr, pseudoStyle.forget(),
                    true, nullptr);
}

bool
nsCSSFrameConstructor::ShouldCreateItemsForChild(nsFrameConstructorState& aState,
                                                 nsIContent* aContent,
                                                 nsContainerFrame* aParentFrame)
{
  aContent->UnsetFlags(NODE_DESCENDANTS_NEED_FRAMES | NODE_NEEDS_FRAME);
  if (aContent->IsElement() && !aContent->IsStyledByServo()) {
    // We can't just remove our pending restyle flags, since we may
    // have restyle-later-siblings set on us.  But we _can_ remove the
    // "is possible restyle root" flags, and need to.  Otherwise we can
    // end up with stale such flags (e.g. if we used to have a
    // display:none parent when our last restyle was posted and
    // processed and now no longer do).
    aContent->UnsetFlags(ELEMENT_ALL_RESTYLE_FLAGS &
                         ~ELEMENT_PENDING_RESTYLE_FLAGS);
  }

  // XXX the GetContent() != aContent check is needed due to bug 135040.
  // Remove it once that's fixed.
  if (aContent->GetPrimaryFrame() &&
      aContent->GetPrimaryFrame()->GetContent() == aContent &&
      !aState.mCreatingExtraFrames) {
    NS_ERROR("asked to create frame construction item for a node that already "
             "has a frame");
    return false;
  }

  // don't create a whitespace frame if aParent doesn't want it
  if (!NeedFrameFor(aState, aParentFrame, aContent)) {
    return false;
  }

  // never create frames for comments or PIs
  if (aContent->IsNodeOfType(nsINode::eCOMMENT) ||
      aContent->IsNodeOfType(nsINode::ePROCESSING_INSTRUCTION)) {
    return false;
  }

  return true;
}

void
nsCSSFrameConstructor::DoAddFrameConstructionItems(nsFrameConstructorState& aState,
                                                   nsIContent* aContent,
                                                   nsStyleContext* aStyleContext,
                                                   bool aSuppressWhiteSpaceOptimizations,
                                                   nsContainerFrame* aParentFrame,
                                                   nsTArray<nsIAnonymousContentCreator::ContentInfo>* aAnonChildren,
                                                   FrameConstructionItemList& aItems)
{
  uint32_t flags = ITEM_ALLOW_XBL_BASE | ITEM_ALLOW_PAGE_BREAK;
  if (aParentFrame) {
    if (aParentFrame->IsSVGText()) {
      flags |= ITEM_IS_WITHIN_SVG_TEXT;
    }
    if (aParentFrame->GetType() == nsGkAtoms::blockFrame &&
        aParentFrame->GetParent() &&
        aParentFrame->GetParent()->GetType() == nsGkAtoms::svgTextFrame) {
      flags |= ITEM_ALLOWS_TEXT_PATH_CHILD;
    }
  }
  AddFrameConstructionItemsInternal(aState, aContent, aParentFrame,
                                    aContent->NodeInfo()->NameAtom(),
                                    aContent->GetNameSpaceID(),
                                    aSuppressWhiteSpaceOptimizations,
                                    aStyleContext,
                                    flags, aAnonChildren,
                                    aItems);
}

void
nsCSSFrameConstructor::AddFrameConstructionItems(nsFrameConstructorState& aState,
                                                 nsIContent* aContent,
                                                 bool aSuppressWhiteSpaceOptimizations,
                                                 const InsertionPoint& aInsertion,
                                                 FrameConstructionItemList& aItems)
{
  nsContainerFrame* parentFrame = aInsertion.mParentFrame;
  if (!ShouldCreateItemsForChild(aState, aContent, parentFrame)) {
    return;
  }
  RefPtr<nsStyleContext> styleContext =
    ResolveStyleContext(aInsertion, aContent, &aState);
  DoAddFrameConstructionItems(aState, aContent, styleContext,
                              aSuppressWhiteSpaceOptimizations, parentFrame,
                              nullptr, aItems);
}

void
nsCSSFrameConstructor::SetAsUndisplayedContent(nsFrameConstructorState& aState,
                                               FrameConstructionItemList& aList,
                                               nsIContent* aContent,
                                               nsStyleContext* aStyleContext,
                                               bool aIsGeneratedContent)
{
  if (aStyleContext->GetPseudo()) {
    if (aIsGeneratedContent) {
      aContent->UnbindFromTree();
    }
    return;
  }
  NS_ASSERTION(!aIsGeneratedContent, "Should have had pseudo type");

  if (aState.mCreatingExtraFrames) {
    MOZ_ASSERT(GetUndisplayedContent(aContent),
               "should have called SetUndisplayedContent earlier");
    return;
  }
  aList.AppendUndisplayedItem(aContent, aStyleContext);
}

void
nsCSSFrameConstructor::AddFrameConstructionItemsInternal(nsFrameConstructorState& aState,
                                                         nsIContent* aContent,
                                                         nsContainerFrame* aParentFrame,
                                                         nsIAtom* aTag,
                                                         int32_t aNameSpaceID,
                                                         bool aSuppressWhiteSpaceOptimizations,
                                                         nsStyleContext* aStyleContext,
                                                         uint32_t aFlags,
                                                         nsTArray<nsIAnonymousContentCreator::ContentInfo>* aAnonChildren,
                                                         FrameConstructionItemList& aItems)
{
  NS_PRECONDITION(aContent->IsNodeOfType(nsINode::eTEXT) ||
                  aContent->IsElement(),
                  "Shouldn't get anything else here!");
  MOZ_ASSERT(!aContent->GetPrimaryFrame() || aState.mCreatingExtraFrames ||
             aContent->NodeInfo()->NameAtom() == nsGkAtoms::area);

  // The following code allows the user to specify the base tag
  // of an element using XBL.  XUL and HTML objects (like boxes, menus, etc.)
  // can then be extended arbitrarily.
  const nsStyleDisplay* display = aStyleContext->StyleDisplay();
  RefPtr<nsStyleContext> styleContext(aStyleContext);
  PendingBinding* pendingBinding = nullptr;
  if ((aFlags & ITEM_ALLOW_XBL_BASE) && display->mBinding)
  {
    // Ensure that our XBL bindings are installed.

    nsXBLService* xblService = nsXBLService::GetInstance();
    if (!xblService)
      return;

    bool resolveStyle;

    nsAutoPtr<PendingBinding> newPendingBinding(new PendingBinding());

    nsresult rv = xblService->LoadBindings(aContent, display->mBinding->GetURI(),
                                           display->mBinding->mOriginPrincipal,
                                           getter_AddRefs(newPendingBinding->mBinding),
                                           &resolveStyle);
    if (NS_FAILED(rv) && rv != NS_ERROR_XBL_BLOCKED)
      return;

    if (newPendingBinding->mBinding) {
      pendingBinding = newPendingBinding;
      // aState takes over owning newPendingBinding
      aState.AddPendingBinding(newPendingBinding.forget());
    }

    if (aContent->IsStyledByServo()) {
      NS_WARNING("stylo: Skipping Unsupported binding re-resolve. This needs fixing.");
      resolveStyle = false;
    }

    if (resolveStyle) {
      styleContext =
        ResolveStyleContext(styleContext->GetParent(), aContent, &aState);
      display = styleContext->StyleDisplay();
      aStyleContext = styleContext;
    }

    aTag = mDocument->BindingManager()->ResolveTag(aContent, &aNameSpaceID);
  }

  bool isGeneratedContent = ((aFlags & ITEM_IS_GENERATED_CONTENT) != 0);

  // Pre-check for display "none" - if we find that, don't create
  // any frame at all
  if (StyleDisplay::None == display->mDisplay) {
    SetAsUndisplayedContent(aState, aItems, aContent, styleContext, isGeneratedContent);
    return;
  }

  bool isText = !aContent->IsElement();

  // never create frames for non-option/optgroup kids of <select> and
  // non-option kids of <optgroup> inside a <select>.
  // XXXbz it's not clear how this should best work with XBL.
  nsIContent *parent = aContent->GetParent();
  if (parent) {
    // Check tag first, since that check will usually fail
    if (parent->IsAnyOfHTMLElements(nsGkAtoms::select, nsGkAtoms::optgroup) &&
        // <option> is ok no matter what
        !aContent->IsHTMLElement(nsGkAtoms::option) &&
        // <optgroup> is OK in <select> but not in <optgroup>
        (!aContent->IsHTMLElement(nsGkAtoms::optgroup) ||
         !parent->IsHTMLElement(nsGkAtoms::select)) &&
        // Allow native anonymous content no matter what
        !aContent->IsRootOfNativeAnonymousSubtree()) {
      // No frame for aContent
      if (!isText) {
        SetAsUndisplayedContent(aState, aItems, aContent, styleContext,
                                isGeneratedContent);
      }
      return;
    }
  }

  // When constructing a child of a non-open <details>, create only the frame
  // for the main <summary> element, and skip other elements.  This only applies
  // to things that are not roots of native anonymous subtrees (except for
  // ::before and ::after); we always want to create "internal" anonymous
  // content.
  auto* details = HTMLDetailsElement::FromContentOrNull(parent);
  if (details && !details->Open() &&
      (!aContent->IsRootOfNativeAnonymousSubtree() ||
       aContent->IsGeneratedContentContainerForBefore() ||
       aContent->IsGeneratedContentContainerForAfter())) {
    auto* summary = HTMLSummaryElement::FromContentOrNull(aContent);
    if (!summary || !summary->IsMainSummary()) {
      SetAsUndisplayedContent(aState, aItems, aContent, styleContext,
                              isGeneratedContent);
      return;
    }
  }

  bool isPopup = false;
  // Try to find frame construction data for this content
  const FrameConstructionData* data;
  if (isText) {
    data = FindTextData(aParentFrame);
    if (!data) {
      // Nothing to do here; suppressed text inside SVG
      return;
    }
  } else {
    Element* element = aContent->AsElement();

    // Don't create frames for non-SVG element children of SVG elements.
    if (aNameSpaceID != kNameSpaceID_SVG &&
        ((aParentFrame &&
          IsFrameForSVG(aParentFrame) &&
          !aParentFrame->IsFrameOfType(nsIFrame::eSVGForeignObject)) ||
         (aFlags & ITEM_IS_WITHIN_SVG_TEXT))) {
      SetAsUndisplayedContent(aState, aItems, element, styleContext,
                              isGeneratedContent);
      return;
    }

    data = FindHTMLData(element, aTag, aNameSpaceID, aParentFrame,
                        styleContext);
    if (!data) {
      data = FindXULTagData(element, aTag, aNameSpaceID, styleContext);
    }
    if (!data) {
      data = FindMathMLData(element, aTag, aNameSpaceID, styleContext);
    }
    if (!data) {
      data = FindSVGData(element, aTag, aNameSpaceID, aParentFrame,
                         aFlags & ITEM_IS_WITHIN_SVG_TEXT,
                         aFlags & ITEM_ALLOWS_TEXT_PATH_CHILD,
                         styleContext);
    }

    // Now check for XUL display types
    if (!data) {
      data = FindXULDisplayData(display, element, styleContext);
    }

    // And general display types
    if (!data) {
      data = FindDisplayData(display, element, styleContext);
    }

    NS_ASSERTION(data, "Should have frame construction data now");

    if (data->mBits & FCDATA_SUPPRESS_FRAME) {
      SetAsUndisplayedContent(aState, aItems, element, styleContext, isGeneratedContent);
      return;
    }

#ifdef MOZ_XUL
    if ((data->mBits & FCDATA_IS_POPUP) &&
        (!aParentFrame || // Parent is inline
         aParentFrame->GetType() != nsGkAtoms::menuFrame)) {
      if (!aState.mPopupItems.containingBlock &&
          !aState.mHavePendingPopupgroup) {
        SetAsUndisplayedContent(aState, aItems, element, styleContext,
                                isGeneratedContent);
        return;
      }

      isPopup = true;
    }
#endif /* MOZ_XUL */
  }

  uint32_t bits = data->mBits;

  // Inside colgroups, suppress everything except columns.
  if (aParentFrame &&
      aParentFrame->GetType() == nsGkAtoms::tableColGroupFrame &&
      (!(bits & FCDATA_IS_TABLE_PART) ||
       display->mDisplay != StyleDisplay::TableColumn)) {
    SetAsUndisplayedContent(aState, aItems, aContent, styleContext, isGeneratedContent);
    return;
  }

  bool canHavePageBreak =
    (aFlags & ITEM_ALLOW_PAGE_BREAK) &&
    aState.mPresContext->IsPaginated() &&
    !display->IsAbsolutelyPositionedStyle() &&
    !(aParentFrame &&
      aParentFrame->GetType() == nsGkAtoms::gridContainerFrame) &&
    !(bits & FCDATA_IS_TABLE_PART) &&
    !(bits & FCDATA_IS_SVG_TEXT);

  if (canHavePageBreak && display->mBreakBefore) {
    AddPageBreakItem(aContent, aStyleContext, aItems);
  }

  if (MOZ_UNLIKELY(bits & FCDATA_IS_CONTENTS)) {
    if (!GetDisplayContentsStyleFor(aContent)) {
      MOZ_ASSERT(styleContext->GetPseudo() || !isGeneratedContent,
                 "Should have had pseudo type");
      aState.mFrameManager->SetDisplayContents(aContent, styleContext);
    } else {
      aState.mFrameManager->ChangeDisplayContents(aContent, styleContext);
    }

    TreeMatchContext::AutoAncestorPusher ancestorPusher(aState.mTreeMatchContext);
    if (aState.mTreeMatchContext.mAncestorFilter.HasFilter()) {
      ancestorPusher.PushAncestorAndStyleScope(aContent->AsElement());
    } else {
      ancestorPusher.PushStyleScope(aContent->AsElement());
    }

    if (aParentFrame) {
      aParentFrame->AddStateBits(NS_FRAME_MAY_HAVE_GENERATED_CONTENT);
    }
    CreateGeneratedContentItem(aState, aParentFrame, aContent, styleContext,
                               CSSPseudoElementType::before, aItems);

    FlattenedChildIterator iter(aContent);
    for (nsIContent* child = iter.GetNextChild(); child; child = iter.GetNextChild()) {
      if (!ShouldCreateItemsForChild(aState, child, aParentFrame)) {
        continue;
      }

      // Get the parent of the content and check if it is a XBL children element
      // (if the content is a children element then parent != aContent because the
      // FlattenedChildIterator will transitively iterate through <xbl:children>
      // for default content). Push the children element as an ancestor here because
      // it does not have a frame and would not otherwise be pushed as an ancestor.
      nsIContent* parent = child->GetParent();
      MOZ_ASSERT(parent, "Parent must be non-null because we are iterating children.");
      TreeMatchContext::AutoAncestorPusher ancestorPusher(aState.mTreeMatchContext);
      if (parent != aContent && parent->IsElement()) {
        if (aState.mTreeMatchContext.mAncestorFilter.HasFilter()) {
          ancestorPusher.PushAncestorAndStyleScope(parent->AsElement());
        } else {
          ancestorPusher.PushStyleScope(parent->AsElement());
        }
      }

      RefPtr<nsStyleContext> childContext =
        ResolveStyleContext(styleContext, child, &aState);
      DoAddFrameConstructionItems(aState, child, childContext,
                                  aSuppressWhiteSpaceOptimizations,
                                  aParentFrame, aAnonChildren, aItems);
    }
    aItems.SetParentHasNoXBLChildren(!iter.XBLInvolved());

    CreateGeneratedContentItem(aState, aParentFrame, aContent, styleContext,
                               CSSPseudoElementType::after, aItems);
    if (canHavePageBreak && display->mBreakAfter) {
      AddPageBreakItem(aContent, aStyleContext, aItems);
    }
    return;
  }

  FrameConstructionItem* item = nullptr;
  if (details && details->Open()) {
    auto* summary = HTMLSummaryElement::FromContentOrNull(aContent);
    if (summary && summary->IsMainSummary()) {
      // If details is open, the main summary needs to be rendered as if it is
      // the first child, so add the item to the front of the item list.
      item = aItems.PrependItem(data, aContent, aTag, aNameSpaceID,
                                pendingBinding, styleContext.forget(),
                                aSuppressWhiteSpaceOptimizations, aAnonChildren);
    }
  }

  if (!item) {
    item = aItems.AppendItem(data, aContent, aTag, aNameSpaceID,
                             pendingBinding, styleContext.forget(),
                             aSuppressWhiteSpaceOptimizations, aAnonChildren);
  }
  item->mIsText = isText;
  item->mIsGeneratedContent = isGeneratedContent;
  item->mIsAnonymousContentCreatorContent =
    aFlags & ITEM_IS_ANONYMOUSCONTENTCREATOR_CONTENT;
  if (isGeneratedContent) {
    NS_ADDREF(item->mContent);
  }
  item->mIsRootPopupgroup =
    aNameSpaceID == kNameSpaceID_XUL && aTag == nsGkAtoms::popupgroup &&
    aContent->IsRootOfNativeAnonymousSubtree();
  if (item->mIsRootPopupgroup) {
    aState.mHavePendingPopupgroup = true;
  }
  item->mIsPopup = isPopup;
  item->mIsForSVGAElement = aNameSpaceID == kNameSpaceID_SVG &&
                            aTag == nsGkAtoms::a;

  if (canHavePageBreak && display->mBreakAfter) {
    AddPageBreakItem(aContent, aStyleContext, aItems);
  }

  if (bits & FCDATA_IS_INLINE) {
    // To correctly set item->mIsAllInline we need to build up our child items
    // right now.
    BuildInlineChildItems(aState, *item,
                          aFlags & ITEM_IS_WITHIN_SVG_TEXT,
                          aFlags & ITEM_ALLOWS_TEXT_PATH_CHILD);
    item->mHasInlineEnds = true;
    item->mIsBlock = false;
  } else {
    // Compute a boolean isInline which is guaranteed to be false for blocks
    // (but may also be false for some inlines).
    bool isInline =
      // Table-internal things are inline-outside if and only if they're kids of
      // inlines, since they'll trigger construction of inline-table
      // pseudos.
      ((bits & FCDATA_IS_TABLE_PART) &&
       (!aParentFrame || // No aParentFrame means inline
        aParentFrame->StyleDisplay()->mDisplay == StyleDisplay::Inline)) ||
      // Things that are inline-outside but aren't inline frames are inline
      display->IsInlineOutsideStyle() ||
      // Popups that are certainly out of flow.
      isPopup;

    // Set mIsAllInline conservatively.  It just might be that even an inline
    // that has mIsAllInline false doesn't need an {ib} split.  So this is just
    // an optimization to keep from doing too much work in cases when we can
    // show that mIsAllInline is true..
    item->mIsAllInline = item->mHasInlineEnds = isInline ||
      // Figure out whether we're guaranteed this item will be out of flow.
      // This is not a precise test, since one of our ancestor inlines might add
      // an absolute containing block (if it's relatively positioned) when there
      // wasn't such a containing block before.  But it's conservative in the
      // sense that anything that will really end up as an in-flow non-inline
      // will test false here.  In other words, if this test is true we're
      // guaranteed to be inline; if it's false we don't know what we'll end up
      // as.
      //
      // If we make this test precise, we can remove some of the code dealing
      // with the imprecision in ConstructInline and adjust the comments on
      // mIsAllInline and mIsBlock in the header.  And probably remove mIsBlock
      // altogether, since then it will always be equal to !mHasInlineEnds.
      (!(bits & FCDATA_DISALLOW_OUT_OF_FLOW) &&
       aState.GetGeometricParent(display, nullptr));

    // Set mIsBlock conservatively.  It's OK to set it false for some real
    // blocks, but not OK to set it true for things that aren't blocks.  Since
    // isOutOfFlow might be false even in cases when the frame will end up
    // out-of-flow, we can't use it here.  But we _can_ say that the frame will
    // for sure end up in-flow if it's not floated or absolutely positioned.
    item->mIsBlock = !isInline &&
                     !display->IsAbsolutelyPositionedStyle() &&
                     !display->IsFloatingStyle() &&
                     !(bits & FCDATA_IS_SVG_TEXT);
  }

  if (item->mIsAllInline) {
    aItems.InlineItemAdded();
  } else if (item->mIsBlock) {
    aItems.BlockItemAdded();
  }

  // Our item should be treated as a line participant if we have the relevant
  // bit and are going to be in-flow.  Note that this really only matters if
  // our ancestor is a box or some such, so the fact that we might have an
  // inline ancestor that might become a containing block is not relevant here.
  if ((bits & FCDATA_IS_LINE_PARTICIPANT) &&
      ((bits & FCDATA_DISALLOW_OUT_OF_FLOW) ||
       !aState.GetGeometricParent(display, nullptr))) {
    item->mIsLineParticipant = true;
    aItems.LineParticipantItemAdded();
  }
}

static void
AddGenConPseudoToFrame(nsIFrame* aOwnerFrame, nsIContent* aContent)
{
  NS_ASSERTION(nsLayoutUtils::IsFirstContinuationOrIBSplitSibling(aOwnerFrame),
               "property should only be set on first continuation/ib-sibling");

  FrameProperties props = aOwnerFrame->Properties();
  nsIFrame::ContentArray* value = props.Get(nsIFrame::GenConProperty());
  if (!value) {
    value = new nsIFrame::ContentArray;
    props.Set(nsIFrame::GenConProperty(), value);
  }
  value->AppendElement(aContent);
}

/**
 * Return true if the frame construction item pointed to by aIter will
 * create a frame adjacent to a line boundary in the frame tree, and that
 * line boundary is induced by a content node adjacent to the frame's
 * content node in the content tree. The latter condition is necessary so
 * that ContentAppended/ContentInserted/ContentRemoved can easily find any
 * text nodes that were suppressed here.
 */
bool
nsCSSFrameConstructor::AtLineBoundary(FCItemIterator& aIter)
{
  if (aIter.item().mSuppressWhiteSpaceOptimizations) {
    return false;
  }

  if (aIter.AtStart()) {
    if (aIter.List()->HasLineBoundaryAtStart() &&
        !aIter.item().mContent->GetPreviousSibling())
      return true;
  } else {
    FCItemIterator prev = aIter;
    prev.Prev();
    if (prev.item().IsLineBoundary() &&
        !prev.item().mSuppressWhiteSpaceOptimizations &&
        aIter.item().mContent->GetPreviousSibling() == prev.item().mContent)
      return true;
  }

  FCItemIterator next = aIter;
  next.Next();
  if (next.IsDone()) {
    if (aIter.List()->HasLineBoundaryAtEnd() &&
        !aIter.item().mContent->GetNextSibling())
      return true;
  } else {
    if (next.item().IsLineBoundary() &&
        !next.item().mSuppressWhiteSpaceOptimizations &&
        aIter.item().mContent->GetNextSibling() == next.item().mContent)
      return true;
  }

  return false;
}

void
nsCSSFrameConstructor::ConstructFramesFromItem(nsFrameConstructorState& aState,
                                               FCItemIterator& aIter,
                                               nsContainerFrame* aParentFrame,
                                               nsFrameItems& aFrameItems)
{
  nsContainerFrame* adjParentFrame = aParentFrame;
  FrameConstructionItem& item = aIter.item();
  nsStyleContext* styleContext = item.mStyleContext;
  AdjustParentFrame(&adjParentFrame, item.mFCData, styleContext);

  if (item.mIsText) {
    // If this is collapsible whitespace next to a line boundary,
    // don't create a frame. item.IsWhitespace() also sets the
    // NS_CREATE_FRAME_IF_NON_WHITESPACE flag in the text node. (If we
    // end up creating a frame, nsTextFrame::Init will clear the flag.)
    // We don't do this for generated content, because some generated
    // text content is empty text nodes that are about to be initialized.
    // (We check mAdditionalStateBits because only the generated content
    // container's frame construction item is marked with
    // mIsGeneratedContent, and we might not have an aParentFrame.)
    // We don't do it for content that may have XBL anonymous siblings,
    // because they make it difficult to correctly create the frame
    // due to dynamic changes.
    // We don't do it for SVG text, since we might need to position and
    // measure the white space glyphs due to x/y/dx/dy attributes.
    if (AtLineBoundary(aIter) &&
        !styleContext->StyleText()->WhiteSpaceOrNewlineIsSignificant() &&
        aIter.List()->ParentHasNoXBLChildren() &&
        !(aState.mAdditionalStateBits & NS_FRAME_GENERATED_CONTENT) &&
        (item.mFCData->mBits & FCDATA_IS_LINE_PARTICIPANT) &&
        !(item.mFCData->mBits & FCDATA_IS_SVG_TEXT) &&
        !mAlwaysCreateFramesForIgnorableWhitespace &&
        item.IsWhitespace(aState))
      return;

    ConstructTextFrame(item.mFCData, aState, item.mContent,
                       adjParentFrame, styleContext,
                       aFrameItems);
    return;
  }

  // Start background loads during frame construction so that we're
  // guaranteed that they will be started before onload fires.
  styleContext->StartBackgroundImageLoads();

  nsFrameState savedStateBits = aState.mAdditionalStateBits;
  if (item.mIsGeneratedContent) {
    // Ensure that frames created here are all tagged with
    // NS_FRAME_GENERATED_CONTENT.
    aState.mAdditionalStateBits |= NS_FRAME_GENERATED_CONTENT;

    // Note that we're not necessarily setting this property on the primary
    // frame for the content for which this is generated content.  We might be
    // setting it on a table pseudo-frame inserted under that instead.  That's
    // OK, though; we just need to do the property set so that the content will
    // get cleaned up when the frame is destroyed.
    ::AddGenConPseudoToFrame(aParentFrame, item.mContent);

    // Now that we've passed ownership of item.mContent to the frame, unset
    // our generated content flag so we don't release or unbind it ourselves.
    item.mIsGeneratedContent = false;
  }

  // XXXbz maybe just inline ConstructFrameFromItemInternal here or something?
  ConstructFrameFromItemInternal(item, aState, adjParentFrame, aFrameItems);

  aState.mAdditionalStateBits = savedStateBits;
}


inline bool
IsRootBoxFrame(nsIFrame *aFrame)
{
  return (aFrame->GetType() == nsGkAtoms::rootFrame);
}

nsresult
nsCSSFrameConstructor::ReconstructDocElementHierarchy()
{
  Element* rootElement = mDocument->GetRootElement();
  if (!rootElement) {
    /* nothing to do */
    return NS_OK;
  }
  return RecreateFramesForContent(rootElement, false, REMOVE_FOR_RECONSTRUCTION,
                                  nullptr);
}

nsContainerFrame*
nsCSSFrameConstructor::GetAbsoluteContainingBlock(nsIFrame* aFrame,
                                                  ContainingBlockType aType)
{
  // Starting with aFrame, look for a frame that is absolutely positioned or
  // relatively positioned (and transformed, if aType is FIXED)
  for (nsIFrame* frame = aFrame; frame; frame = frame->GetParent()) {
    if (frame->IsFrameOfType(nsIFrame::eMathML)) {
      // If it's mathml, bail out -- no absolute positioning out from inside
      // mathml frames.  Note that we don't make this part of the loop
      // condition because of the stuff at the end of this method...
      return nullptr;
    }

    // Look for the ICB.
    if (aType == FIXED_POS) {
      nsIAtom* t = frame->GetType();
      if (t == nsGkAtoms::viewportFrame ||
          t == nsGkAtoms::pageContentFrame) {
        return static_cast<nsContainerFrame*>(frame);
      }
    }

    // If the frame is positioned, we will probably return it as the containing
    // block (see the exceptions below).  Otherwise, we'll start looking at the
    // parent frame, unless we're dealing with a scrollframe.
    // Scrollframes are special since they're not positioned, but their
    // scrolledframe might be.  So, we need to check this special case to return
    // the correct containing block (the scrolledframe) in that case.
    // If we're looking for a fixed-pos containing block and the frame is
    // not transformed, skip it.
    if (!frame->IsAbsPosContainingBlock() ||
        (aType == FIXED_POS &&
         !frame->IsFixedPosContainingBlock())) {
      continue;
    }
    nsIFrame* absPosCBCandidate = frame;
    nsIAtom* type = absPosCBCandidate->GetType();
    if (type == nsGkAtoms::fieldSetFrame) {
      absPosCBCandidate = static_cast<nsFieldSetFrame*>(absPosCBCandidate)->GetInner();
      if (!absPosCBCandidate) {
        continue;
      }
      type = absPosCBCandidate->GetType();
    }
    if (type == nsGkAtoms::scrollFrame) {
      nsIScrollableFrame* scrollFrame = do_QueryFrame(absPosCBCandidate);
      absPosCBCandidate = scrollFrame->GetScrolledFrame();
      if (!absPosCBCandidate) {
        continue;
      }
      type = absPosCBCandidate->GetType();
    }
    // Only first continuations can be containing blocks.
    absPosCBCandidate = absPosCBCandidate->FirstContinuation();
    // Is the frame really an absolute container?
    if (!absPosCBCandidate->IsAbsoluteContainer()) {
      continue;
    }

    // For tables, skip the inner frame and consider the table wrapper frame.
    if (type == nsGkAtoms::tableFrame) {
      continue;
    }
    // For table wrapper frames, we can just return absPosCBCandidate.
    MOZ_ASSERT((nsContainerFrame*)do_QueryFrame(absPosCBCandidate),
               "abs.pos. containing block must be nsContainerFrame sub-class");
    return static_cast<nsContainerFrame*>(absPosCBCandidate);
  }

  MOZ_ASSERT(aType != FIXED_POS, "no ICB in this frame tree?");

  // It is possible for the search for the containing block to fail, because
  // no absolute container can be found in the parent chain.  In those cases,
  // we fall back to the document element's containing block.
  return mHasRootAbsPosContainingBlock ? mDocElementContainingBlock : nullptr;
}

nsContainerFrame*
nsCSSFrameConstructor::GetFloatContainingBlock(nsIFrame* aFrame)
{
  // Starting with aFrame, look for a frame that is a float containing block.
  // IF we hit a mathml frame, bail out; we don't allow floating out of mathml
  // frames, because they don't seem to be able to deal.
  // The logic here needs to match the logic in ProcessChildren()
  for (nsIFrame* containingBlock = aFrame;
       containingBlock &&
         !ShouldSuppressFloatingOfDescendants(containingBlock);
       containingBlock = containingBlock->GetParent()) {
    if (containingBlock->IsFloatContainingBlock()) {
      MOZ_ASSERT((nsContainerFrame*)do_QueryFrame(containingBlock),
                 "float containing block must be nsContainerFrame sub-class");
      return static_cast<nsContainerFrame*>(containingBlock);
    }
  }

  // If we didn't find a containing block, then there just isn't
  // one.... return null
  return nullptr;
}

/**
 * This function will check whether aContainer has :after generated content.
 * If so, appending to it should actually insert.  The return value is the
 * parent to use for newly-appended content.  *aAfterFrame points to the :after
 * frame before which appended content should go, if there is one.
 */
static nsContainerFrame*
AdjustAppendParentForAfterContent(nsFrameManager* aFrameManager,
                                  nsIContent* aContainer,
                                  nsContainerFrame* aParentFrame,
                                  nsIContent* aChild,
                                  nsIFrame** aAfterFrame)
{
  // If the parent frame has any pseudo-elements or aContainer is a
  // display:contents node then we need to walk through the child
  // frames to find the first one that is either a ::after frame for an
  // ancestor of aChild or a frame that is for a node later in the
  // document than aChild and return that in aAfterFrame.
  if (aParentFrame->GetGenConPseudos() ||
      nsLayoutUtils::HasPseudoStyle(aContainer, aParentFrame->StyleContext(),
                                    CSSPseudoElementType::after,
                                    aParentFrame->PresContext()) ||
      aFrameManager->GetDisplayContentsStyleFor(aContainer)) {
    nsIFrame* afterFrame = nullptr;
    nsContainerFrame* parent =
      static_cast<nsContainerFrame*>(aParentFrame->LastContinuation());
    bool done = false;
    while (!done && parent) {
      // Ensure that all normal flow children are on the principal child list.
      parent->DrainSelfOverflowList();

      nsIFrame* child = parent->GetChildList(nsIFrame::kPrincipalList).LastChild();
      if (child && child->IsPseudoFrame(aContainer) &&
          !child->IsGeneratedContentFrame()) {
        // Drill down into non-generated pseudo frames of aContainer.
        nsContainerFrame* childAsContainer = do_QueryFrame(child);
        if (childAsContainer) {
          parent = nsLayoutUtils::LastContinuationWithChild(childAsContainer);
          continue;
        }
      }

      for (; child; child = child->GetPrevSibling()) {
        nsIContent* c = child->GetContent();
        if (child->IsGeneratedContentFrame()) {
          nsIContent* p = c->GetParent();
          if (c->NodeInfo()->NameAtom() == nsGkAtoms::mozgeneratedcontentafter) {
            if (!nsContentUtils::ContentIsDescendantOf(aChild, p) &&
                p != aContainer &&
                nsContentUtils::PositionIsBefore(p, aChild)) {
              // ::after generated content for content earlier in the doc and not
              // for an ancestor.  "p != aContainer" may seem redundant but it
              // checks if the ::after belongs to the XBL insertion point we're
              // inserting aChild into (in which case ContentIsDescendantOf is
              // false even though p == aContainer).
              // See layout/reftests/bugs/482592-1a.xhtml for an example of that.
              done = true;
              break;
            }
          } else if (nsContentUtils::PositionIsBefore(p, aChild)) {
            // Non-::after generated content for content earlier in the doc.
            done = true;
            break;
          }
        } else if (nsContentUtils::PositionIsBefore(c, aChild)) {
          // Content is before aChild.
          done = true;
          break;
        }
        afterFrame = child;
      }

      parent = static_cast<nsContainerFrame*>(parent->GetPrevContinuation());
    }
    if (afterFrame) {
      *aAfterFrame = afterFrame;
      return afterFrame->GetParent();
    }
  }

  *aAfterFrame = nullptr;

  if (IsFramePartOfIBSplit(aParentFrame)) {
    // We might be in a situation where the last part of the {ib} split was
    // empty.  Since we have no ::after pseudo-element, we do in fact want to be
    // appending to that last part, so advance to it if needed.  Note that here
    // aParentFrame is the result of a GetLastIBSplitSibling call, so must be
    // either the last or next to last ib-split sibling.
    nsContainerFrame* trailingInline = GetIBSplitSibling(aParentFrame);
    if (trailingInline) {
      aParentFrame = trailingInline;
    }

    // Always make sure to look at the last continuation of the frame
    // for the {ib} case, even if that continuation is empty.  We
    // don't do this for the non-ib-split-frame case, since in the
    // other cases appending to the last nonempty continuation is fine
    // and in fact not doing that can confuse code that doesn't know
    // to pull kids from continuations other than its next one.
    aParentFrame =
      static_cast<nsContainerFrame*>(aParentFrame->LastContinuation());
  }

  return aParentFrame;
}

/**
 * This function will get the previous sibling to use for an append operation.
 * it takes a parent frame (must not be null) and its :after frame (may be
 * null).
 */
static nsIFrame*
FindAppendPrevSibling(nsIFrame* aParentFrame, nsIFrame* aAfterFrame)
{
  if (aAfterFrame) {
    NS_ASSERTION(aAfterFrame->GetParent() == aParentFrame, "Wrong parent");
    NS_ASSERTION(aAfterFrame->GetPrevSibling() ||
                 aParentFrame->PrincipalChildList().FirstChild() == aAfterFrame,
                 ":after frame must be on the principal child list here");
    return aAfterFrame->GetPrevSibling();
  }

  aParentFrame->DrainSelfOverflowList();

  return aParentFrame->GetChildList(kPrincipalList).LastChild();
}

/**
 * This function will get the next sibling for a frame insert operation given
 * the parent and previous sibling.  aPrevSibling may be null.
 */
static nsIFrame*
GetInsertNextSibling(nsIFrame* aParentFrame, nsIFrame* aPrevSibling)
{
  if (aPrevSibling) {
    return aPrevSibling->GetNextSibling();
  }

  return aParentFrame->PrincipalChildList().FirstChild();
}

/**
 * This function is called by ContentAppended() and ContentInserted() when
 * appending flowed frames to a parent's principal child list. It handles the
 * case where the parent is the trailing inline of an {ib} split.
 */
nsresult
nsCSSFrameConstructor::AppendFramesToParent(nsFrameConstructorState&       aState,
                                            nsContainerFrame*              aParentFrame,
                                            nsFrameItems&                  aFrameList,
                                            nsIFrame*                      aPrevSibling,
                                            bool                           aIsRecursiveCall)
{
  NS_PRECONDITION(!IsFramePartOfIBSplit(aParentFrame) ||
                  !GetIBSplitSibling(aParentFrame) ||
                  !GetIBSplitSibling(aParentFrame)->PrincipalChildList().FirstChild(),
                  "aParentFrame has a ib-split sibling with kids?");
  NS_PRECONDITION(!aPrevSibling || aPrevSibling->GetParent() == aParentFrame,
                  "Parent and prevsibling don't match");

  nsIFrame* nextSibling = ::GetInsertNextSibling(aParentFrame, aPrevSibling);

  NS_ASSERTION(nextSibling ||
               !aParentFrame->GetNextContinuation() ||
               !aParentFrame->GetNextContinuation()->PrincipalChildList().FirstChild() ||
               aIsRecursiveCall,
               "aParentFrame has later continuations with kids?");
  NS_ASSERTION(nextSibling ||
               !IsFramePartOfIBSplit(aParentFrame) ||
               (IsInlineFrame(aParentFrame) &&
                !GetIBSplitSibling(aParentFrame) &&
                !aParentFrame->GetNextContinuation()) ||
               aIsRecursiveCall,
               "aParentFrame is not last?");

  // If we're inserting a list of frames at the end of the trailing inline
  // of an {ib} split, we may need to create additional {ib} siblings to parent
  // them.
  if (!nextSibling && IsFramePartOfIBSplit(aParentFrame)) {
    // When we get here, our frame list might start with a block.  If it does
    // so, and aParentFrame is an inline, and it and all its previous
    // continuations have no siblings, then put the initial blocks from the
    // frame list into the previous block of the {ib} split.  Note that we
    // didn't want to stop at the block part of the split when figuring out
    // initial parent, because that could screw up float parenting; it's easier
    // to do this little fixup here instead.
    if (aFrameList.NotEmpty() && !aFrameList.FirstChild()->IsInlineOutside()) {
      // See whether our trailing inline is empty
      nsIFrame* firstContinuation = aParentFrame->FirstContinuation();
      if (firstContinuation->PrincipalChildList().IsEmpty()) {
        // Our trailing inline is empty.  Collect our starting blocks from
        // aFrameList, get the right parent frame for them, and put them in.
        nsFrameList::FrameLinkEnumerator firstNonBlockEnumerator =
          FindFirstNonBlock(aFrameList);
        nsFrameList blockKids = aFrameList.ExtractHead(firstNonBlockEnumerator);
        NS_ASSERTION(blockKids.NotEmpty(), "No blocks?");

        nsContainerFrame* prevBlock = GetIBSplitPrevSibling(firstContinuation);
        prevBlock = static_cast<nsContainerFrame*>(prevBlock->LastContinuation());
        NS_ASSERTION(prevBlock, "Should have previous block here");

        MoveChildrenTo(aParentFrame, prevBlock, blockKids);
      }
    }

    // We want to put some of the frames into this inline frame.
    nsFrameList::FrameLinkEnumerator firstBlockEnumerator(aFrameList);
    FindFirstBlock(firstBlockEnumerator);

    nsFrameList inlineKids = aFrameList.ExtractHead(firstBlockEnumerator);
    if (!inlineKids.IsEmpty()) {
      AppendFrames(aParentFrame, kPrincipalList, inlineKids);
    }

    if (!aFrameList.IsEmpty()) {
      bool positioned = aParentFrame->IsRelativelyPositioned();
      nsFrameItems ibSiblings;
      CreateIBSiblings(aState, aParentFrame, positioned, aFrameList,
                       ibSiblings);

      // Make sure to trigger reflow of the inline that used to be our
      // last one and now isn't anymore, since its GetSkipSides() has
      // changed.
      mPresShell->FrameNeedsReflow(aParentFrame,
                                   nsIPresShell::eTreeChange,
                                   NS_FRAME_HAS_DIRTY_CHILDREN);

      // Recurse so we create new ib siblings as needed for aParentFrame's parent
      return AppendFramesToParent(aState, aParentFrame->GetParent(), ibSiblings,
                                  aParentFrame, true);
    }

    return NS_OK;
  }

  // Insert the frames after our aPrevSibling
  InsertFrames(aParentFrame, kPrincipalList, aPrevSibling, aFrameList);
  return NS_OK;
}

#define UNSET_DISPLAY static_cast<StyleDisplay>(255)

// This gets called to see if the frames corresponding to aSibling and aContent
// should be siblings in the frame tree. Although (1) rows and cols, (2) row
// groups and col groups, (3) row groups and captions, (4) legends and content
// inside fieldsets, (5) popups and other kids of the menu are siblings from a
// content perspective, they are not considered siblings in the frame tree.
bool
nsCSSFrameConstructor::IsValidSibling(nsIFrame*              aSibling,
                                      nsIContent*            aContent,
                                      StyleDisplay&          aDisplay)
{
  nsIFrame* parentFrame = aSibling->GetParent();
  nsIAtom* parentType = parentFrame->GetType();

  StyleDisplay siblingDisplay = aSibling->GetDisplay();
  if (StyleDisplay::TableColumnGroup == siblingDisplay ||
      StyleDisplay::TableColumn      == siblingDisplay ||
      StyleDisplay::TableCaption     == siblingDisplay ||
      StyleDisplay::TableHeaderGroup == siblingDisplay ||
      StyleDisplay::TableRowGroup    == siblingDisplay ||
      StyleDisplay::TableFooterGroup == siblingDisplay ||
      nsGkAtoms::menuFrame == parentType) {
    // if we haven't already, construct a style context to find the display type of aContent
    if (UNSET_DISPLAY == aDisplay) {
      nsIFrame* styleParent;
      aSibling->GetParentStyleContext(&styleParent);
      if (!styleParent) {
        styleParent = aSibling->GetParent();
      }
      if (!styleParent) {
        NS_NOTREACHED("Shouldn't happen");
        return false;
      }
      if (aContent->IsNodeOfType(nsINode::eCOMMENT) ||
          aContent->IsNodeOfType(nsINode::ePROCESSING_INSTRUCTION)) {
        // Comments and processing instructions never have frames, so we
        // should not try to generate style contexts for them.
        return false;
      }
      // XXXbz when this code is killed, the state argument to
      // ResolveStyleContext can be made non-optional.
      RefPtr<nsStyleContext> styleContext =
        ResolveStyleContext(styleParent, aContent, nullptr);
      const nsStyleDisplay* display = styleContext->StyleDisplay();
      aDisplay = display->mDisplay;
    }
    if (nsGkAtoms::menuFrame == parentType) {
      return
        (StyleDisplay::Popup == aDisplay) ==
        (StyleDisplay::Popup == siblingDisplay);
    }
    // To have decent performance we want to return false in cases in which
    // reordering the two siblings has no effect on display.  To ensure
    // correctness, we MUST return false in cases where the two siblings have
    // the same desired parent type and live on different display lists.
    // Specificaly, columns and column groups should only consider columns and
    // column groups as valid siblings.  Captions should only consider other
    // captions.  All other things should consider each other as valid
    // siblings.  The restriction in the |if| above on siblingDisplay is ok,
    // because for correctness the only part that really needs to happen is to
    // not consider captions, column groups, and row/header/footer groups
    // siblings of each other.  Treating a column or colgroup as a valid
    // sibling of a non-table-related frame will just mean we end up reframing.
    if ((siblingDisplay == StyleDisplay::TableCaption) !=
        (aDisplay == StyleDisplay::TableCaption)) {
      // One's a caption and the other is not.  Not valid siblings.
      return false;
    }

    if ((siblingDisplay == StyleDisplay::TableColumnGroup ||
         siblingDisplay == StyleDisplay::TableColumn) !=
        (aDisplay == StyleDisplay::TableColumnGroup ||
         aDisplay == StyleDisplay::TableColumn)) {
      // One's a column or column group and the other is not.  Not valid
      // siblings.
      return false;
    }
    // Fall through; it's possible that the display type was overridden and
    // a different sort of frame was constructed, so we may need to return false
    // below.
  }

  if (IsFrameForFieldSet(parentFrame, parentType)) {
    // Legends can be sibling of legends but not of other content in the fieldset
    if (nsContainerFrame* cif = aSibling->GetContentInsertionFrame()) {
      aSibling = cif;
    }
    nsIAtom* sibType = aSibling->GetType();
    bool legendContent = aContent->IsHTMLElement(nsGkAtoms::legend);

    if ((legendContent  && (nsGkAtoms::legendFrame != sibType)) ||
        (!legendContent && (nsGkAtoms::legendFrame == sibType)))
      return false;
  }

  return true;
}

nsIFrame*
nsCSSFrameConstructor::FindFrameForContentSibling(nsIContent* aContent,
                                                  nsIContent* aTargetContent,
                                                  StyleDisplay& aTargetContentDisplay,
                                                  nsContainerFrame* aParentFrame,
                                                  bool aPrevSibling)
{
  nsIFrame* sibling = aContent->GetPrimaryFrame();
  if (!sibling && GetDisplayContentsStyleFor(aContent)) {
    // A display:contents node - check if it has a ::before / ::after frame...
    sibling = aPrevSibling ?
      nsLayoutUtils::GetAfterFrameForContent(aParentFrame, aContent) :
      nsLayoutUtils::GetBeforeFrameForContent(aParentFrame, aContent);
    if (!sibling) {
      // ... then recurse into children ...
      const bool forward = !aPrevSibling;
      FlattenedChildIterator iter(aContent, forward);
      sibling = aPrevSibling ?
        FindPreviousSibling(iter, aTargetContent, aTargetContentDisplay, aParentFrame) :
        FindNextSibling(iter, aTargetContent, aTargetContentDisplay, aParentFrame);
    }
    if (!sibling) {
      // ... then ::after / ::before on the opposite end.
      sibling = aPrevSibling ?
        nsLayoutUtils::GetBeforeFrameForContent(aParentFrame, aContent) :
        nsLayoutUtils::GetAfterFrameForContent(aParentFrame, aContent);
    }
    if (!sibling) {
      return nullptr;
    }
  } else if (!sibling || sibling->GetContent() != aContent) {
    // XXX the GetContent() != aContent check is needed due to bug 135040.
    // Remove it once that's fixed.
    return nullptr;
  }

  // If the frame is out-of-flow, GetPrimaryFrame() will have returned the
  // out-of-flow frame; we want the placeholder.
  if (sibling->GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
    nsIFrame* placeholderFrame = GetPlaceholderFrameFor(sibling);
    NS_ASSERTION(placeholderFrame, "no placeholder for out-of-flow frame");
    sibling = placeholderFrame;
  }

  // The frame we have now should never be a continuation
  NS_ASSERTION(!sibling->GetPrevContinuation(), "How did that happen?");

  if (aPrevSibling) {
    // The frame may be a ib-split frame (a split inline frame that
    // contains a block).  Get the last part of that split.
    if (IsFramePartOfIBSplit(sibling)) {
      sibling = GetLastIBSplitSibling(sibling, true);
    }

    // The frame may have a continuation. If so, we want the last
    // non-overflow-container continuation as our previous sibling.
    sibling = sibling->GetTailContinuation();
  }

  if (aTargetContent &&
      !IsValidSibling(sibling, aTargetContent, aTargetContentDisplay)) {
    sibling = nullptr;
  }

  return sibling;
}

nsIFrame*
nsCSSFrameConstructor::FindPreviousSibling(FlattenedChildIterator aIter,
                                           nsIContent* aTargetContent,
                                           StyleDisplay& aTargetContentDisplay,
                                           nsContainerFrame* aParentFrame)
{
  // Note: not all content objects are associated with a frame (e.g., if it's
  // `display: none') so keep looking until we find a previous frame.
  while (nsIContent* sibling = aIter.GetPreviousChild()) {
    MOZ_ASSERT(sibling != aTargetContent);
    nsIFrame* prevSibling =
      FindFrameForContentSibling(sibling, aTargetContent, aTargetContentDisplay,
                                 aParentFrame, true);
    if (prevSibling) {
      // Found a previous sibling, we're done!
      return prevSibling;
    }
  }

  return nullptr;
}

nsIFrame*
nsCSSFrameConstructor::FindNextSibling(FlattenedChildIterator aIter,
                                       nsIContent* aTargetContent,
                                       StyleDisplay& aTargetContentDisplay,
                                       nsContainerFrame* aParentFrame)
{
  while (nsIContent* sibling = aIter.GetNextChild()) {
    MOZ_ASSERT(sibling != aTargetContent);
    nsIFrame* nextSibling =
      FindFrameForContentSibling(sibling, aTargetContent, aTargetContentDisplay,
                                 aParentFrame, false);

    if (nextSibling) {
      // We found a next sibling, we're done!
      return nextSibling;
    }
  }

  return nullptr;
}

// For fieldsets, returns the area frame, if the child is not a legend.
static nsContainerFrame*
GetAdjustedParentFrame(nsContainerFrame* aParentFrame,
                       nsIAtom*          aParentFrameType,
                       nsIContent*       aChildContent)
{
  NS_PRECONDITION(nsGkAtoms::tableWrapperFrame != aParentFrameType,
                  "Shouldn't be happening!");

  nsContainerFrame* newParent = nullptr;

  if (nsGkAtoms::fieldSetFrame == aParentFrameType) {
    // If the parent is a fieldSet, use the fieldSet's area frame as the
    // parent unless the new content is a legend.
    if (!aChildContent->IsHTMLElement(nsGkAtoms::legend)) {
      newParent = GetFieldSetBlockFrame(aParentFrame);
    }
  }
  return newParent ? newParent : aParentFrame;
}

nsIFrame*
nsCSSFrameConstructor::GetInsertionPrevSibling(InsertionPoint* aInsertion,
                                               nsIContent* aChild,
                                               bool*       aIsAppend,
                                               bool*       aIsRangeInsertSafe,
                                               nsIContent* aStartSkipChild,
                                               nsIContent* aEndSkipChild)
{
  NS_PRECONDITION(aInsertion->mParentFrame, "Must have parent frame to start with");

  *aIsAppend = false;

  // Find the frame that precedes the insertion point. Walk backwards
  // from the parent frame to get the parent content, because if an
  // XBL insertion point is involved, we'll need to use _that_ to find
  // the preceding frame.
  FlattenedChildIterator iter(aInsertion->mContainer);
  bool xblCase = iter.XBLInvolved() ||
         aInsertion->mParentFrame->GetContent() != aInsertion->mContainer;
  if (xblCase || !aChild->IsRootOfAnonymousSubtree()) {
    // The check for IsRootOfAnonymousSubtree() is because editor is
    // severely broken and calls us directly for native anonymous
    // nodes that it creates.
    if (aStartSkipChild) {
      iter.Seek(aStartSkipChild);
    } else {
      iter.Seek(aChild);
    }
  } else {
    // Prime the iterator for the call to FindPreviousSibling.
    iter.GetNextChild();
    MOZ_ASSERT(aChild->GetProperty(nsGkAtoms::restylableAnonymousNode),
               "Someone passed native anonymous content directly into frame "
               "construction.  Stop doing that!");
  }

  // Note that FindPreviousSibling is passed the iterator by value, so that
  // the later usage of the iterator starts from the same place.
  StyleDisplay childDisplay = UNSET_DISPLAY;
  nsIFrame* prevSibling =
    FindPreviousSibling(iter, iter.Get(), childDisplay, aInsertion->mParentFrame);

  // Now, find the geometric parent so that we can handle
  // continuations properly. Use the prev sibling if we have it;
  // otherwise use the next sibling.
  if (prevSibling) {
    aInsertion->mParentFrame = prevSibling->GetParent()->GetContentInsertionFrame();
  } else {
    // If there is no previous sibling, then find the frame that follows
    if (aEndSkipChild) {
      iter.Seek(aEndSkipChild);
      iter.GetPreviousChild();
    }
    nsIFrame* nextSibling =
      FindNextSibling(iter, iter.Get(), childDisplay, aInsertion->mParentFrame);
    if (GetDisplayContentsStyleFor(aInsertion->mContainer)) {
      if (!nextSibling) {
        // Our siblings (if any) does not have a frame to guide us.
        // The frame for aChild should be inserted whereever a frame for
        // the container would be inserted.  This is needed when inserting
        // into nested display:contents nodes.
        nsIContent* child = aInsertion->mContainer;
        nsIContent* parent = child->GetParent();
        aInsertion->mParentFrame =
          ::GetAdjustedParentFrame(aInsertion->mParentFrame,
                                   aInsertion->mParentFrame->GetType(),
                                   parent);
        InsertionPoint fakeInsertion(aInsertion->mParentFrame, parent);
        nsIFrame* result = GetInsertionPrevSibling(&fakeInsertion, child, aIsAppend,
                                                   aIsRangeInsertSafe, nullptr, nullptr);
        MOZ_ASSERT(aInsertion->mParentFrame->GetContent() ==
                   fakeInsertion.mParentFrame->GetContent());
        // fakeInsertion.mParentFrame may now be a continuation of the frame
        // we started with in the ctor above.
        aInsertion->mParentFrame = fakeInsertion.mParentFrame;
        return result;
      }

      prevSibling = nextSibling->GetPrevSibling();
    }

    if (nextSibling) {
      aInsertion->mParentFrame = nextSibling->GetParent()->GetContentInsertionFrame();
    } else {
      // No previous or next sibling, so treat this like an appended frame.
      *aIsAppend = true;
      if (IsFramePartOfIBSplit(aInsertion->mParentFrame)) {
        // Since we're appending, we'll walk to the last anonymous frame
        // that was created for the broken inline frame.  But don't walk
        // to the trailing inline if it's empty; stop at the block.
        aInsertion->mParentFrame =
          GetLastIBSplitSibling(aInsertion->mParentFrame, false);
      }
      // Get continuation that parents the last child.  This MUST be done
      // before the AdjustAppendParentForAfterContent call.
      aInsertion->mParentFrame =
        nsLayoutUtils::LastContinuationWithChild(aInsertion->mParentFrame);
      // Deal with fieldsets
      aInsertion->mParentFrame =
        ::GetAdjustedParentFrame(aInsertion->mParentFrame,
                                 aInsertion->mParentFrame->GetType(),
                                 aChild);
      nsIFrame* appendAfterFrame;
      aInsertion->mParentFrame =
        ::AdjustAppendParentForAfterContent(this, aInsertion->mContainer,
                                            aInsertion->mParentFrame,
                                            aChild, &appendAfterFrame);
      prevSibling = ::FindAppendPrevSibling(aInsertion->mParentFrame, appendAfterFrame);
    }
  }

  *aIsRangeInsertSafe = (childDisplay == UNSET_DISPLAY);
  return prevSibling;
}

nsContainerFrame*
nsCSSFrameConstructor::GetContentInsertionFrameFor(nsIContent* aContent)
{
  // Get the primary frame associated with the content
  nsIFrame* frame = aContent->GetPrimaryFrame();

  if (!frame) {
    if (GetDisplayContentsStyleFor(aContent)) {
      nsIContent* parent = aContent->GetParent();
      if (parent && parent == aContent->GetContainingShadow()) {
        parent = parent->GetBindingParent();
      }
      frame = parent ? GetContentInsertionFrameFor(parent) : nullptr;
    }
    if (!frame) {
      return nullptr;
    }
  } else {
    // If the content of the frame is not the desired content then this is not
    // really a frame for the desired content.
    // XXX This check is needed due to bug 135040. Remove it once that's fixed.
    if (frame->GetContent() != aContent) {
      return nullptr;
    }
  }

  nsContainerFrame* insertionFrame = frame->GetContentInsertionFrame();

  NS_ASSERTION(!insertionFrame || insertionFrame == frame || !frame->IsLeaf(),
    "The insertion frame is the primary frame or the primary frame isn't a leaf");

  return insertionFrame;
}

static bool
IsSpecialFramesetChild(nsIContent* aContent)
{
  // IMPORTANT: This must match the conditions in nsHTMLFramesetFrame::Init.
  return aContent->IsAnyOfHTMLElements(nsGkAtoms::frameset, nsGkAtoms::frame);
}

static void
InvalidateCanvasIfNeeded(nsIPresShell* presShell, nsIContent* node);

#ifdef MOZ_XUL

static
bool
IsXULListBox(nsIContent* aContainer)
{
  return (aContainer->IsXULElement(nsGkAtoms::listbox));
}

static
nsListBoxBodyFrame*
MaybeGetListBoxBodyFrame(nsIContent* aContainer, nsIContent* aChild)
{
  if (!aContainer)
    return nullptr;

  if (IsXULListBox(aContainer) &&
      aChild->IsXULElement(nsGkAtoms::listitem)) {
    nsCOMPtr<nsIDOMXULElement> xulElement = do_QueryInterface(aContainer);
    nsCOMPtr<nsIBoxObject> boxObject;
    xulElement->GetBoxObject(getter_AddRefs(boxObject));
    nsCOMPtr<nsPIListBoxObject> listBoxObject = do_QueryInterface(boxObject);
    if (listBoxObject) {
      return listBoxObject->GetListBoxBody(false);
    }
  }

  return nullptr;
}
#endif

void
nsCSSFrameConstructor::AddTextItemIfNeeded(nsFrameConstructorState& aState,
                                           const InsertionPoint& aInsertion,
                                           nsIContent* aPossibleTextContent,
                                           FrameConstructionItemList& aItems)
{
  NS_PRECONDITION(aPossibleTextContent, "Must have node");
  if (!aPossibleTextContent->IsNodeOfType(nsINode::eTEXT) ||
      !aPossibleTextContent->HasFlag(NS_CREATE_FRAME_IF_NON_WHITESPACE)) {
    // Not text, or not suppressed due to being all-whitespace (if it
    // were being suppressed, it would have the
    // NS_CREATE_FRAME_IF_NON_WHITESPACE flag)
    return;
  }
  NS_ASSERTION(!aPossibleTextContent->GetPrimaryFrame(),
               "Text node has a frame and NS_CREATE_FRAME_IF_NON_WHITESPACE");
  AddFrameConstructionItems(aState, aPossibleTextContent, false,
                            aInsertion, aItems);
}

void
nsCSSFrameConstructor::ReframeTextIfNeeded(nsIContent* aParentContent,
                                           nsIContent* aContent)
{
  if (!aContent->IsNodeOfType(nsINode::eTEXT) ||
      !aContent->HasFlag(NS_CREATE_FRAME_IF_NON_WHITESPACE)) {
    // Not text, or not suppressed due to being all-whitespace (if it
    // were being suppressed, it would have the
    // NS_CREATE_FRAME_IF_NON_WHITESPACE flag)
    return;
  }
  NS_ASSERTION(!aContent->GetPrimaryFrame(),
               "Text node has a frame and NS_CREATE_FRAME_IF_NON_WHITESPACE");
  ContentInserted(aParentContent, aContent, nullptr, false);
}

// For inserts aChild should be valid, for appends it should be null.
// Returns true if this operation can be lazy, false if not.
bool
nsCSSFrameConstructor::MaybeConstructLazily(Operation aOperation,
                                            nsIContent* aContainer,
                                            nsIContent* aChild)
{
  if (mPresShell->GetPresContext()->IsChrome() || !aContainer ||
      aContainer->IsInNativeAnonymousSubtree() || aContainer->IsXULElement()) {
    return false;
  }

  if (aOperation == CONTENTINSERT) {
    if (aChild->IsRootOfAnonymousSubtree() ||
        (aChild->HasFlag(NODE_IS_IN_SHADOW_TREE) &&
         !aChild->IsInNativeAnonymousSubtree()) ||
        aChild->IsEditable() || aChild->IsXULElement()) {
      return false;
    }
  } else { // CONTENTAPPEND
    NS_ASSERTION(aOperation == CONTENTAPPEND,
                 "operation should be either insert or append");
    for (nsIContent* child = aChild; child; child = child->GetNextSibling()) {
      NS_ASSERTION(!child->IsRootOfAnonymousSubtree(),
                   "Should be coming through the CONTENTAPPEND case");
      if (child->IsXULElement() || child->IsEditable()) {
        return false;
      }
    }
  }

  // We can construct lazily; just need to set suitable bits in the content
  // tree.

  // Walk up the tree setting the NODE_DESCENDANTS_NEED_FRAMES bit as we go.
  nsIContent* content = aContainer;
#ifdef DEBUG
  // If we hit a node with no primary frame, or the NODE_NEEDS_FRAME bit set
  // we want to assert, but leaf frames that process their own children and may
  // ignore anonymous children (eg framesets) make this complicated. So we set
  // these two booleans if we encounter these situations and unset them if we
  // hit a node with a leaf frame.
  bool noPrimaryFrame = false;
  bool needsFrameBitSet = false;
#endif
  while (content &&
         !content->HasFlag(NODE_DESCENDANTS_NEED_FRAMES)) {
#ifdef DEBUG
    if (content->GetPrimaryFrame() && content->GetPrimaryFrame()->IsLeaf()) {
      noPrimaryFrame = needsFrameBitSet = false;
    }
    if (!noPrimaryFrame && !content->GetPrimaryFrame()) {
      noPrimaryFrame = true;
    }
    if (!needsFrameBitSet && content->HasFlag(NODE_NEEDS_FRAME)) {
      needsFrameBitSet = true;
    }
#endif
    // XXXmats no lazy frames for display:contents descendants yet (bug 979782).
    if (GetDisplayContentsStyleFor(content)) {
      return false;
    }
    content->SetFlags(NODE_DESCENDANTS_NEED_FRAMES);
    content = content->GetFlattenedTreeParent();
  }
#ifdef DEBUG
  if (content && content->GetPrimaryFrame() &&
      content->GetPrimaryFrame()->IsLeaf()) {
    noPrimaryFrame = needsFrameBitSet = false;
  }
  NS_ASSERTION(!noPrimaryFrame, "Ancestors of nodes with frames to be "
    "constructed lazily should have frames");
  NS_ASSERTION(!needsFrameBitSet, "Ancestors of nodes with frames to be "
    "constructed lazily should not have NEEDS_FRAME bit set");
#endif

  // Set NODE_NEEDS_FRAME on the new nodes.
  if (aOperation == CONTENTINSERT) {
    NS_ASSERTION(!aChild->GetPrimaryFrame() ||
                 aChild->GetPrimaryFrame()->GetContent() != aChild,
                 //XXX the aChild->GetPrimaryFrame()->GetContent() != aChild
                 // check is needed due to bug 135040. Remove it once that's
                 // fixed.
                 "setting NEEDS_FRAME on a node that already has a frame?");
    aChild->SetFlags(NODE_NEEDS_FRAME);
  } else { // CONTENTAPPEND
    for (nsIContent* child = aChild; child; child = child->GetNextSibling()) {
      NS_ASSERTION(!child->GetPrimaryFrame() ||
                   child->GetPrimaryFrame()->GetContent() != child,
                   //XXX the child->GetPrimaryFrame()->GetContent() != child
                   // check is needed due to bug 135040. Remove it once that's
                   // fixed.
                   "setting NEEDS_FRAME on a node that already has a frame?");
      child->SetFlags(NODE_NEEDS_FRAME);
    }
  }

  RestyleManager()->PostRestyleEventForLazyConstruction();
  return true;
}

void
nsCSSFrameConstructor::CreateNeededFrames(nsIContent* aContent)
{
  NS_ASSERTION(!aContent->HasFlag(NODE_NEEDS_FRAME),
    "shouldn't get here with a content node that has needs frame bit set");
  NS_ASSERTION(aContent->HasFlag(NODE_DESCENDANTS_NEED_FRAMES),
    "should only get here with a content node that has descendants needing frames");

  aContent->UnsetFlags(NODE_DESCENDANTS_NEED_FRAMES);

  // We could either descend first (on nodes that don't have NODE_NEEDS_FRAME
  // set) or issue content notifications for our kids first. In absence of
  // anything definitive either way we'll go with the latter.

  // It might be better to use GetChildArray and scan it completely first and
  // then issue all notifications. (We have to scan it completely first because
  // constructing frames can set attributes, which can change the storage of
  // child lists).

  // Scan the children of aContent to see what operations (if any) we need to
  // perform.
  uint32_t childCount = aContent->GetChildCount();
  bool inRun = false;
  nsIContent* firstChildInRun = nullptr;
  for (uint32_t i = 0; i < childCount; i++) {
    nsIContent* child = aContent->GetChildAt(i);
    if (child->HasFlag(NODE_NEEDS_FRAME)) {
      NS_ASSERTION(!child->GetPrimaryFrame() ||
                   child->GetPrimaryFrame()->GetContent() != child,
                   //XXX the child->GetPrimaryFrame()->GetContent() != child
                   // check is needed due to bug 135040. Remove it once that's
                   // fixed.
                   "NEEDS_FRAME set on a node that already has a frame?");
      if (!inRun) {
        inRun = true;
        firstChildInRun = child;
      }
    } else {
      if (inRun) {
        inRun = false;
        // generate a ContentRangeInserted for [startOfRun,i)
        ContentRangeInserted(aContent, firstChildInRun, child, nullptr,
                             false);
      }
    }
  }
  if (inRun) {
    ContentAppended(aContent, firstChildInRun, false);
  }

  // Now descend.
  FlattenedChildIterator iter(aContent);
  for (nsIContent* child = iter.GetNextChild(); child; child = iter.GetNextChild()) {
    if (child->HasFlag(NODE_DESCENDANTS_NEED_FRAMES)) {
      CreateNeededFrames(child);
    }
  }
}

void nsCSSFrameConstructor::CreateNeededFrames()
{
  NS_ASSERTION(!nsContentUtils::IsSafeToRunScript(),
               "Someone forgot a script blocker");

  Element* rootElement = mDocument->GetRootElement();
  NS_ASSERTION(!rootElement || !rootElement->HasFlag(NODE_NEEDS_FRAME),
    "root element should not have frame created lazily");
  if (rootElement && rootElement->HasFlag(NODE_DESCENDANTS_NEED_FRAMES)) {
    BeginUpdate();
    CreateNeededFrames(rootElement);
    EndUpdate();
  }
}

void
nsCSSFrameConstructor::IssueSingleInsertNofications(nsIContent* aContainer,
                                                    nsIContent* aStartChild,
                                                    nsIContent* aEndChild,
                                                    bool aAllowLazyConstruction)
{
  for (nsIContent* child = aStartChild;
       child != aEndChild;
       child = child->GetNextSibling()) {
    if ((child->GetPrimaryFrame() || GetUndisplayedContent(child) ||
         GetDisplayContentsStyleFor(child))
#ifdef MOZ_XUL
        //  Except listboxes suck, so do NOT skip anything here if
        //  we plan to notify a listbox.
        && !MaybeGetListBoxBodyFrame(aContainer, child)
#endif
        ) {
      // Already have a frame or undisplayed entry for this content; a
      // previous ContentInserted in this loop must have reconstructed
      // its insertion parent.  Skip it.
      continue;
    }
    // Call ContentInserted with this node.
    ContentInserted(aContainer, child, mTempFrameTreeState,
                    aAllowLazyConstruction);
  }
}

nsCSSFrameConstructor::InsertionPoint
nsCSSFrameConstructor::GetRangeInsertionPoint(nsIContent* aContainer,
                                              nsIContent* aStartChild,
                                              nsIContent* aEndChild,
                                              bool aAllowLazyConstruction)
{
  // See if we have an XBL insertion point. If so, then that's our
  // real parent frame; if not, then the frame hasn't been built yet
  // and we just bail.
  InsertionPoint insertionPoint = GetInsertionPoint(aContainer, nullptr);
  if (!insertionPoint.mParentFrame && !insertionPoint.mMultiple) {
    return insertionPoint; // Don't build the frames.
  }

  bool hasInsertion = false;
  if (!insertionPoint.mMultiple) {
    // XXXbz XBL2/sXBL issue
    nsIDocument* document = aStartChild->GetComposedDoc();
    // XXXbz how would |document| be null here?
    if (document && aStartChild->GetXBLInsertionParent()) {
      hasInsertion = true;
    }
  }

  if (insertionPoint.mMultiple || hasInsertion) {
    // We have an insertion point.  There are some additional tests we need to do
    // in order to ensure that an append is a safe operation.
    uint32_t childCount = 0;

    if (!insertionPoint.mMultiple) {
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
      // XXXndeakin This test doesn't work in the new world. Or rather, it works, but
      // it's slow
      childCount = insertionPoint.mParentFrame->GetContent()->GetChildCount();
    }

    // If we have multiple insertion points or if we have an insertion point
    // and the operation is not a true append or if the insertion point already
    // has explicit children, then we must fall back.
    if (insertionPoint.mMultiple || aEndChild != nullptr || childCount > 0) {
      // Now comes the fun part.  For each inserted child, make a
      // ContentInserted call as if it had just gotten inserted and
      // let ContentInserted handle the mess.
      IssueSingleInsertNofications(aContainer, aStartChild, aEndChild,
                                   aAllowLazyConstruction);
      insertionPoint.mParentFrame = nullptr;
    }
  }

  return insertionPoint;
}

bool
nsCSSFrameConstructor::MaybeRecreateForFrameset(nsIFrame* aParentFrame,
                                                nsIContent* aStartChild,
                                                nsIContent* aEndChild)
{
  if (aParentFrame->GetType() == nsGkAtoms::frameSetFrame) {
    // Check whether we have any kids we care about.
    for (nsIContent* cur = aStartChild;
         cur != aEndChild;
         cur = cur->GetNextSibling()) {
      if (IsSpecialFramesetChild(cur)) {
        // Just reframe the parent, since framesets are weird like that.
        RecreateFramesForContent(aParentFrame->GetContent(), false,
                                 REMOVE_FOR_RECONSTRUCTION, nullptr);
        return true;
      }
    }
  }
  return false;
}

nsresult
nsCSSFrameConstructor::ContentAppended(nsIContent*     aContainer,
                                       nsIContent*     aFirstNewContent,
                                       bool            aAllowLazyConstruction)
{
  AUTO_LAYOUT_PHASE_ENTRY_POINT(mPresShell->GetPresContext(), FrameC);
  NS_PRECONDITION(mUpdateCount != 0,
                  "Should be in an update while creating frames");

#ifdef DEBUG
  if (gNoisyContentUpdates) {
    printf("nsCSSFrameConstructor::ContentAppended container=%p "
           "first-child=%p lazy=%d\n",
           static_cast<void*>(aContainer), aFirstNewContent,
           aAllowLazyConstruction);
    if (gReallyNoisyContentUpdates && aContainer) {
      aContainer->List(stdout, 0);
    }
  }
#endif

#ifdef DEBUG
  for (nsIContent* child = aFirstNewContent;
       child;
       child = child->GetNextSibling()) {
    // XXX the GetContent() != child check is needed due to bug 135040.
    // Remove it once that's fixed.
    NS_ASSERTION(!child->GetPrimaryFrame() ||
                 child->GetPrimaryFrame()->GetContent() != child,
                 "asked to construct a frame for a node that already has a frame");
  }
#endif

#ifdef MOZ_XUL
  if (aContainer) {
    int32_t namespaceID;
    nsIAtom* tag =
      mDocument->BindingManager()->ResolveTag(aContainer, &namespaceID);

    // Just ignore tree tags, anyway we don't create any frames for them.
    if (tag == nsGkAtoms::treechildren ||
        tag == nsGkAtoms::treeitem ||
        tag == nsGkAtoms::treerow)
      return NS_OK;

  }
#endif // MOZ_XUL

  if (aContainer && aContainer->HasFlag(NODE_IS_IN_SHADOW_TREE) &&
      !aContainer->IsInNativeAnonymousSubtree() &&
      !aFirstNewContent->IsInNativeAnonymousSubtree()) {
    // Recreate frames if content is appended into a ShadowRoot
    // because children of ShadowRoot are rendered in place of children
    // of the host.
    //XXXsmaug This is super unefficient!
    nsIContent* bindingParent = aContainer->GetBindingParent();
    LAYOUT_PHASE_TEMP_EXIT();
    nsresult rv = RecreateFramesForContent(bindingParent, false,
                                           REMOVE_FOR_RECONSTRUCTION, nullptr);
    LAYOUT_PHASE_TEMP_REENTER();
    return rv;
  }

  // See comment in ContentRangeInserted for why this is necessary.
  if (!GetContentInsertionFrameFor(aContainer) &&
      !aContainer->IsActiveChildrenElement()) {
    return NS_OK;
  }

  if (aAllowLazyConstruction &&
      MaybeConstructLazily(CONTENTAPPEND, aContainer, aFirstNewContent)) {
    if (aContainer->IsStyledByServo()) {
      aContainer->AsElement()->NoteDirtyDescendantsForServo();
    }
    return NS_OK;
  }

  // We couldn't construct lazily. Make Servo eagerly traverse the subtree.
  if (ServoStyleSet* set = mPresShell->StyleSet()->GetAsServo()) {
    // We use the same codepaths to handle both of the following cases:
    //   (a) Newly-appended content for which lazy frame construction is disallowed.
    //   (b) Lazy frame construction driven by the restyle manager.
    // We need the styles for (a). In the case of (b), the Servo traversal has
    // already happened, so we don't need to do it again.
    if (!RestyleManager()->AsBase()->IsInStyleRefresh()) {
      if (aFirstNewContent->GetNextSibling()) {
        set->StyleNewChildren(aContainer);
      } else {
        set->StyleNewSubtree(aFirstNewContent);
      }
    }
  }

  LAYOUT_PHASE_TEMP_EXIT();
  InsertionPoint insertion =
    GetRangeInsertionPoint(aContainer, aFirstNewContent, nullptr,
                           aAllowLazyConstruction);
  nsContainerFrame*& parentFrame = insertion.mParentFrame;
  LAYOUT_PHASE_TEMP_REENTER();
  if (!parentFrame) {
    return NS_OK;
  }

  LAYOUT_PHASE_TEMP_EXIT();
  if (MaybeRecreateForFrameset(parentFrame, aFirstNewContent, nullptr)) {
    LAYOUT_PHASE_TEMP_REENTER();
    return NS_OK;
  }
  LAYOUT_PHASE_TEMP_REENTER();

  if (parentFrame->IsLeaf()) {
    // Nothing to do here; we shouldn't be constructing kids of leaves
    // Clear lazy bits so we don't try to construct again.
    ClearLazyBits(aFirstNewContent, nullptr);
    return NS_OK;
  }

  if (parentFrame->IsFrameOfType(nsIFrame::eMathML)) {
    LAYOUT_PHASE_TEMP_EXIT();
    nsresult rv = RecreateFramesForContent(parentFrame->GetContent(), false,
                                           REMOVE_FOR_RECONSTRUCTION, nullptr);
    LAYOUT_PHASE_TEMP_REENTER();
    return rv;
  }

  // If the frame we are manipulating is a ib-split frame (that is, one
  // that's been created as a result of a block-in-inline situation) then we
  // need to append to the last ib-split sibling, not to the frame itself.
  bool parentIBSplit = IsFramePartOfIBSplit(parentFrame);
  if (parentIBSplit) {
#ifdef DEBUG
    if (gNoisyContentUpdates) {
      printf("nsCSSFrameConstructor::ContentAppended: parentFrame=");
      nsFrame::ListTag(stdout, parentFrame);
      printf(" is ib-split\n");
    }
#endif

    // Since we're appending, we'll walk to the last anonymous frame
    // that was created for the broken inline frame.  But don't walk
    // to the trailing inline if it's empty; stop at the block.
    parentFrame = GetLastIBSplitSibling(parentFrame, false);
  }

  // Get continuation that parents the last child.  This MUST be done
  // before the AdjustAppendParentForAfterContent call.
  parentFrame = nsLayoutUtils::LastContinuationWithChild(parentFrame);

  // We should never get here with fieldsets or details, since they have
  // multiple insertion points.
  MOZ_ASSERT(parentFrame->GetType() != nsGkAtoms::fieldSetFrame &&
             parentFrame->GetType() != nsGkAtoms::detailsFrame,
             "Parent frame should not be fieldset or details!");

  // Deal with possible :after generated content on the parent
  nsIFrame* parentAfterFrame;
  parentFrame =
    ::AdjustAppendParentForAfterContent(this, insertion.mContainer, parentFrame,
                                        aFirstNewContent, &parentAfterFrame);

  // Create some new frames
  nsFrameConstructorState state(mPresShell,
                                GetAbsoluteContainingBlock(parentFrame, FIXED_POS),
                                GetAbsoluteContainingBlock(parentFrame, ABS_POS),
                                GetFloatContainingBlock(parentFrame));
  state.mTreeMatchContext.InitAncestors(aContainer->AsElement());

  // See if the containing block has :first-letter style applied.
  bool haveFirstLetterStyle = false, haveFirstLineStyle = false;
  nsContainerFrame* containingBlock = state.mFloatedItems.containingBlock;
  if (containingBlock) {
    haveFirstLetterStyle = HasFirstLetterStyle(containingBlock);
    haveFirstLineStyle =
      ShouldHaveFirstLineStyle(containingBlock->GetContent(),
                               containingBlock->StyleContext());
  }

  if (haveFirstLetterStyle) {
    // Before we get going, remove the current letter frames
    RemoveLetterFrames(state.mPresShell, containingBlock);
  }

  nsIAtom* frameType = parentFrame->GetType();

  FlattenedChildIterator iter(aContainer);
  bool haveNoXBLChildren = (!iter.XBLInvolved() || !iter.GetNextChild());
  FrameConstructionItemList items;
  if (aFirstNewContent->GetPreviousSibling() &&
      GetParentType(frameType) == eTypeBlock &&
      haveNoXBLChildren) {
    // If there's a text node in the normal content list just before the new
    // items, and it has no frame, make a frame construction item for it. If it
    // doesn't need a frame, ConstructFramesFromItemList below won't give it
    // one.  No need to do all this if our parent type is not block, though,
    // since WipeContainingBlock already handles that situation.
    //
    // Because we're appending, we don't need to worry about any text
    // after the appended content; there can only be XBL anonymous content
    // (text in an XBL binding is not suppressed) or generated content
    // (and bare text nodes are not generated). Native anonymous content
    // generated by frames never participates in inline layout.
    AddTextItemIfNeeded(state, insertion,
                        aFirstNewContent->GetPreviousSibling(), items);
  }
  for (nsIContent* child = aFirstNewContent;
       child;
       child = child->GetNextSibling()) {
    AddFrameConstructionItems(state, child, false, insertion, items);
  }

  nsIFrame* prevSibling = ::FindAppendPrevSibling(parentFrame, parentAfterFrame);

  // Perform special check for diddling around with the frames in
  // a ib-split inline frame.
  // If we're appending before :after content, then we're not really
  // appending, so let WipeContainingBlock know that.
  LAYOUT_PHASE_TEMP_EXIT();
  if (WipeContainingBlock(state, containingBlock, parentFrame, items,
                          true, prevSibling)) {
    LAYOUT_PHASE_TEMP_REENTER();
    return NS_OK;
  }
  LAYOUT_PHASE_TEMP_REENTER();

  // If the parent is a block frame, and we're not in a special case
  // where frames can be moved around, determine if the list is for the
  // start or end of the block.
  if (nsLayoutUtils::GetAsBlock(parentFrame) && !haveFirstLetterStyle &&
      !haveFirstLineStyle && !parentIBSplit) {
    items.SetLineBoundaryAtStart(!prevSibling ||
        !prevSibling->IsInlineOutside() ||
        prevSibling->GetType() == nsGkAtoms::brFrame);
    // :after content can't be <br> so no need to check it
    items.SetLineBoundaryAtEnd(!parentAfterFrame ||
        !parentAfterFrame->IsInlineOutside());
  }
  // To suppress whitespace-only text frames, we have to verify that
  // our container's DOM child list matches its flattened tree child list.
  items.SetParentHasNoXBLChildren(haveNoXBLChildren);

  nsFrameItems frameItems;
  ConstructFramesFromItemList(state, items, parentFrame, frameItems);

  for (nsIContent* child = aFirstNewContent;
       child;
       child = child->GetNextSibling()) {
    // Invalidate now instead of before the WipeContainingBlock call, just in
    // case we do wipe; in that case we don't need to do this walk at all.
    // XXXbz does that matter?  Would it make more sense to save some virtual
    // GetChildAt calls instead and do this during construction of our
    // FrameConstructionItemList?
    InvalidateCanvasIfNeeded(mPresShell, child);
  }

  // If the container is a table and a caption was appended, it needs to be put
  // in the table wrapper frame's additional child list.
  nsFrameItems captionItems;
  if (nsGkAtoms::tableFrame == frameType) {
    // Pull out the captions.  Note that we don't want to do that as we go,
    // because processing a single caption can add a whole bunch of things to
    // the frame items due to pseudoframe processing.  So we'd have to pull
    // captions from a list anyway; might as well do that here.
    // XXXbz this is no longer true; we could pull captions directly out of the
    // FrameConstructionItemList now.
    PullOutCaptionFrames(frameItems, captionItems);
  }

  if (haveFirstLineStyle && parentFrame == containingBlock) {
    // It's possible that some of the new frames go into a
    // first-line frame. Look at them and see...
    AppendFirstLineFrames(state, containingBlock->GetContent(),
                          containingBlock, frameItems);
  }

  // Notify the parent frame passing it the list of new frames
  // Append the flowed frames to the principal child list; captions
  // need special treatment
  if (captionItems.NotEmpty()) { // append the caption to the table wrapper
    NS_ASSERTION(nsGkAtoms::tableFrame == frameType, "how did that happen?");
    nsContainerFrame* outerTable = parentFrame->GetParent();
    AppendFrames(outerTable, nsIFrame::kCaptionList, captionItems);
  }

  if (frameItems.NotEmpty()) { // append the in-flow kids
    AppendFramesToParent(state, parentFrame, frameItems, prevSibling);
  }

  // Recover first-letter frames
  if (haveFirstLetterStyle) {
    RecoverLetterFrames(containingBlock);
  }

#ifdef DEBUG
  if (gReallyNoisyContentUpdates) {
    printf("nsCSSFrameConstructor::ContentAppended: resulting frame model:\n");
    parentFrame->List(stdout, 0);
  }
#endif

#ifdef ACCESSIBILITY
  nsAccessibilityService* accService = nsIPresShell::AccService();
  if (accService) {
    accService->ContentRangeInserted(mPresShell, aContainer,
                                     aFirstNewContent, nullptr);
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
bool NotifyListBoxBody(nsPresContext*    aPresContext,
                         nsIContent*        aContainer,
                         nsIContent*        aChild,
                         // Only used for the removed notification
                         nsIContent*        aOldNextSibling,
                         nsIFrame*          aChildFrame,
                         content_operation  aOperation)
{
  nsListBoxBodyFrame* listBoxBodyFrame =
    MaybeGetListBoxBodyFrame(aContainer, aChild);
  if (listBoxBodyFrame) {
    if (aOperation == CONTENT_REMOVED) {
      // Except if we have an aChildFrame and its parent is not the right
      // thing, then we don't do this.  Pseudo frames are so much fun....
      if (!aChildFrame || aChildFrame->GetParent() == listBoxBodyFrame) {
        listBoxBodyFrame->OnContentRemoved(aPresContext, aContainer,
                                           aChildFrame, aOldNextSibling);
        return true;
      }
    } else {
      listBoxBodyFrame->OnContentInserted(aChild);
      return true;
    }
  }

  return false;
}
#endif // MOZ_XUL

nsresult
nsCSSFrameConstructor::ContentInserted(nsIContent*            aContainer,
                                       nsIContent*            aChild,
                                       nsILayoutHistoryState* aFrameState,
                                       bool                   aAllowLazyConstruction)
{
  return ContentRangeInserted(aContainer,
                              aChild,
                              aChild->GetNextSibling(),
                              aFrameState,
                              aAllowLazyConstruction);
}

// ContentRangeInserted handles creating frames for a range of nodes that
// aren't at the end of their childlist. ContentRangeInserted isn't a real
// content notification, but rather it handles regular ContentInserted calls
// for a single node as well as the lazy construction of frames for a range of
// nodes when called from CreateNeededFrames. For a range of nodes to be
// suitable to have its frames constructed all at once they must meet the same
// conditions that ContentAppended imposes (GetRangeInsertionPoint checks
// these), plus more. Namely when finding the insertion prevsibling we must not
// need to consult something specific to any one node in the range, so that the
// insertion prevsibling would be the same for each node in the range. So we
// pass the first node in the range to GetInsertionPrevSibling, and if
// IsValidSibling (the only place GetInsertionPrevSibling might look at the
// passed in node itself) needs to resolve style on the node we record this and
// return that this range needs to be split up and inserted separately. Table
// captions need extra attention as we need to determine where to insert them
// in the caption list, while skipping any nodes in the range being inserted
// (because when we treat the caption frames the other nodes have had their
// frames constructed but not yet inserted into the frame tree).
nsresult
nsCSSFrameConstructor::ContentRangeInserted(nsIContent*            aContainer,
                                            nsIContent*            aStartChild,
                                            nsIContent*            aEndChild,
                                            nsILayoutHistoryState* aFrameState,
                                            bool                   aAllowLazyConstruction)
{
  AUTO_LAYOUT_PHASE_ENTRY_POINT(mPresShell->GetPresContext(), FrameC);
  NS_PRECONDITION(mUpdateCount != 0,
                  "Should be in an update while creating frames");

  NS_PRECONDITION(aStartChild, "must always pass a child");

  // XXXldb Do we need to re-resolve style to handle the CSS2 + combinator and
  // the :empty pseudo-class?
#ifdef DEBUG
  if (gNoisyContentUpdates) {
    printf("nsCSSFrameConstructor::ContentRangeInserted container=%p "
           "start-child=%p end-child=%p lazy=%d\n",
           static_cast<void*>(aContainer),
           static_cast<void*>(aStartChild), static_cast<void*>(aEndChild),
           aAllowLazyConstruction);
    if (gReallyNoisyContentUpdates) {
      if (aContainer) {
        aContainer->List(stdout,0);
      } else {
        aStartChild->List(stdout, 0);
      }
    }
  }
#endif

#ifdef DEBUG
  for (nsIContent* child = aStartChild;
       child != aEndChild;
       child = child->GetNextSibling()) {
    // XXX the GetContent() != child check is needed due to bug 135040.
    // Remove it once that's fixed.
    NS_ASSERTION(!child->GetPrimaryFrame() ||
                 child->GetPrimaryFrame()->GetContent() != child,
                 "asked to construct a frame for a node that already has a frame");
  }
#endif

  bool isSingleInsert = (aStartChild->GetNextSibling() == aEndChild);
  NS_ASSERTION(isSingleInsert || !aAllowLazyConstruction,
               "range insert shouldn't be lazy");
  NS_ASSERTION(isSingleInsert || aEndChild,
               "range should not include all nodes after aStartChild");

#ifdef MOZ_XUL
  if (aContainer && IsXULListBox(aContainer)) {
    if (isSingleInsert) {
      if (NotifyListBoxBody(mPresShell->GetPresContext(), aContainer,
                            // The insert case in NotifyListBoxBody
                            // doesn't use "old next sibling".
                            aStartChild, nullptr, nullptr, CONTENT_INSERTED)) {
        return NS_OK;
      }
    } else {
      // We don't handle a range insert to a listbox parent, issue single
      // ContertInserted calls for each node inserted.
      LAYOUT_PHASE_TEMP_EXIT();
      IssueSingleInsertNofications(aContainer, aStartChild, aEndChild,
                                   aAllowLazyConstruction);
      LAYOUT_PHASE_TEMP_REENTER();
      return NS_OK;
    }
  }
#endif // MOZ_XUL

  // If we have a null parent, then this must be the document element being
  // inserted, or some other child of the document in the DOM (might be a PI,
  // say).
  if (! aContainer) {
    NS_ASSERTION(isSingleInsert,
                 "root node insertion should be a single insertion");
    Element *docElement = mDocument->GetRootElement();

    if (aStartChild != docElement) {
      // Not the root element; just bail out
      return NS_OK;
    }

    NS_PRECONDITION(nullptr == mRootElementFrame,
                    "root element frame already created");

    // Create frames for the document element and its child elements
    nsIFrame* docElementFrame =
      ConstructDocElementFrame(docElement, aFrameState);

    if (docElementFrame) {
      InvalidateCanvasIfNeeded(mPresShell, aStartChild);
#ifdef DEBUG
      if (gReallyNoisyContentUpdates) {
        printf("nsCSSFrameConstructor::ContentRangeInserted: resulting frame "
               "model:\n");
        docElementFrame->List(stdout, 0);
      }
#endif
    }

    if (aFrameState) {
      // Restore frame state for the root scroll frame if there is one
      nsIFrame* rootScrollFrame = mPresShell->GetRootScrollFrame();
      if (rootScrollFrame) {
        RestoreFrameStateFor(rootScrollFrame, aFrameState);
      }
    }

#ifdef ACCESSIBILITY
    nsAccessibilityService* accService = nsIPresShell::AccService();
    if (accService) {
      accService->ContentRangeInserted(mPresShell, aContainer,
                                       aStartChild, aEndChild);
    }
#endif

    return NS_OK;
  }

  if (aContainer->HasFlag(NODE_IS_IN_SHADOW_TREE) &&
      !aContainer->IsInNativeAnonymousSubtree() &&
      (!aStartChild || !aStartChild->IsInNativeAnonymousSubtree()) &&
      (!aEndChild || !aEndChild->IsInNativeAnonymousSubtree())) {
    // Recreate frames if content is inserted into a ShadowRoot
    // because children of ShadowRoot are rendered in place of
    // the children of the host.
    //XXXsmaug This is super unefficient!
    nsIContent* bindingParent = aContainer->GetBindingParent();
    LAYOUT_PHASE_TEMP_EXIT();
    nsresult rv = RecreateFramesForContent(bindingParent, false,
                                           REMOVE_FOR_RECONSTRUCTION, nullptr);
    LAYOUT_PHASE_TEMP_REENTER();
    return rv;
  }

  // Put 'parentFrame' inside a scope so we don't confuse it with
  // 'insertion.mParentFrame' later.
  {
    nsContainerFrame* parentFrame = GetContentInsertionFrameFor(aContainer);
    // The xbl:children element won't have a frame, but default content can have the children as
    // a parent. While its uncommon to change the structure of the default content itself, a label,
    // for example, can be reframed by having its value attribute set or removed.
    if (!parentFrame && !aContainer->IsActiveChildrenElement()) {
      return NS_OK;
    }

    // Otherwise, we've got parent content. Find its frame.
    NS_ASSERTION(!parentFrame || parentFrame->GetContent() == aContainer ||
                 GetDisplayContentsStyleFor(aContainer), "New XBL code is possibly wrong!");

    if (aAllowLazyConstruction &&
        MaybeConstructLazily(CONTENTINSERT, aContainer, aStartChild)) {
      if (aContainer->IsStyledByServo()) {
        aContainer->AsElement()->NoteDirtyDescendantsForServo();
      }
      return NS_OK;
    }
  }

  // We couldn't construct lazily. Make Servo eagerly traverse the subtree.
  if (ServoStyleSet* set = mPresShell->StyleSet()->GetAsServo()) {
    // We use the same codepaths to handle both of the following cases:
    //   (a) Newly-appended content for which lazy frame construction is disallowed.
    //   (b) Lazy frame construction driven by the restyle manager.
    // We need the styles for (a). In the case of (b), the Servo traversal has
    // already happened, so we don't need to do it again.
    if (!RestyleManager()->AsBase()->IsInStyleRefresh()) {
      set->StyleNewSubtree(aStartChild);
    }
  }

  InsertionPoint insertion;
  if (isSingleInsert) {
    // See if we have an XBL insertion point. If so, then that's our
    // real parent frame; if not, then the frame hasn't been built yet
    // and we just bail.
    insertion = GetInsertionPoint(aContainer, aStartChild);
  } else {
    // Get our insertion point. If we need to issue single ContentInserted's
    // GetRangeInsertionPoint will take care of that for us.
    LAYOUT_PHASE_TEMP_EXIT();
    insertion = GetRangeInsertionPoint(aContainer, aStartChild, aEndChild,
                                       aAllowLazyConstruction);
    LAYOUT_PHASE_TEMP_REENTER();
  }

  if (!insertion.mParentFrame) {
    return NS_OK;
  }

  bool isAppend, isRangeInsertSafe;
  nsIFrame* prevSibling = GetInsertionPrevSibling(&insertion, aStartChild,
                                                  &isAppend, &isRangeInsertSafe);

  // check if range insert is safe
  if (!isSingleInsert && !isRangeInsertSafe) {
    // must fall back to a single ContertInserted for each child in the range
    LAYOUT_PHASE_TEMP_EXIT();
    IssueSingleInsertNofications(aContainer, aStartChild, aEndChild,
                                 aAllowLazyConstruction);
    LAYOUT_PHASE_TEMP_REENTER();
    return NS_OK;
  }

  nsIContent* container = insertion.mParentFrame->GetContent();

  nsIAtom* frameType = insertion.mParentFrame->GetType();
  LAYOUT_PHASE_TEMP_EXIT();
  if (MaybeRecreateForFrameset(insertion.mParentFrame, aStartChild, aEndChild)) {
    LAYOUT_PHASE_TEMP_REENTER();
    return NS_OK;
  }
  LAYOUT_PHASE_TEMP_REENTER();

  // We should only get here with fieldsets when doing a single insert, because
  // fieldsets have multiple insertion points.
  NS_ASSERTION(isSingleInsert || frameType != nsGkAtoms::fieldSetFrame,
               "Unexpected parent");
  if (IsFrameForFieldSet(insertion.mParentFrame, frameType) &&
      aStartChild->NodeInfo()->NameAtom() == nsGkAtoms::legend) {
    // Just reframe the parent, since figuring out whether this
    // should be the new legend and then handling it is too complex.
    // We could do a little better here --- check if the fieldset already
    // has a legend which occurs earlier in its child list than this node,
    // and if so, proceed. But we'd have to extend nsFieldSetFrame
    // to locate this legend in the inserted frames and extract it.
    LAYOUT_PHASE_TEMP_EXIT();
    nsresult rv = RecreateFramesForContent(insertion.mParentFrame->GetContent(), false,
                                           REMOVE_FOR_RECONSTRUCTION, nullptr);
    LAYOUT_PHASE_TEMP_REENTER();
    return rv;
  }

  // We should only get here with details when doing a single insertion because
  // we treat details frame as if it has multiple insertion points.
  MOZ_ASSERT(isSingleInsert || frameType != nsGkAtoms::detailsFrame);
  if (frameType == nsGkAtoms::detailsFrame) {
    // When inserting an element into <details>, just reframe the details frame
    // and let it figure out where the element should be laid out. It might seem
    // expensive to recreate the entire details frame, but it's the simplest way
    // to handle the insertion.
    LAYOUT_PHASE_TEMP_EXIT();
    nsresult rv =
      RecreateFramesForContent(insertion.mParentFrame->GetContent(), false,
                               REMOVE_FOR_RECONSTRUCTION, nullptr);
    LAYOUT_PHASE_TEMP_REENTER();
    return rv;
  }

  // Don't construct kids of leaves
  if (insertion.mParentFrame->IsLeaf()) {
    // Clear lazy bits so we don't try to construct again.
    ClearLazyBits(aStartChild, aEndChild);
    return NS_OK;
  }

  if (insertion.mParentFrame->IsFrameOfType(nsIFrame::eMathML)) {
    LAYOUT_PHASE_TEMP_EXIT();
    nsresult rv = RecreateFramesForContent(insertion.mParentFrame->GetContent(), false,
                                           REMOVE_FOR_RECONSTRUCTION, nullptr);
    LAYOUT_PHASE_TEMP_REENTER();
    return rv;
  }

  nsFrameConstructorState state(mPresShell,
                                GetAbsoluteContainingBlock(insertion.mParentFrame, FIXED_POS),
                                GetAbsoluteContainingBlock(insertion.mParentFrame, ABS_POS),
                                GetFloatContainingBlock(insertion.mParentFrame),
                                do_AddRef(aFrameState));
  state.mTreeMatchContext.InitAncestors(aContainer ?
                                          aContainer->AsElement() :
                                          nullptr);

  // Recover state for the containing block - we need to know if
  // it has :first-letter or :first-line style applied to it. The
  // reason we care is that the internal structure in these cases
  // is not the normal structure and requires custom updating
  // logic.
  nsContainerFrame* containingBlock = state.mFloatedItems.containingBlock;
  bool haveFirstLetterStyle = false;
  bool haveFirstLineStyle = false;

  // In order to shave off some cycles, we only dig up the
  // containing block haveFirst* flags if the parent frame where
  // the insertion/append is occurring is an inline or block
  // container. For other types of containers this isn't relevant.
  StyleDisplay parentDisplay = insertion.mParentFrame->GetDisplay();

  // Examine the insertion.mParentFrame where the insertion is taking
  // place. If it's a certain kind of container then some special
  // processing is done.
  if ((StyleDisplay::Block == parentDisplay) ||
      (StyleDisplay::ListItem == parentDisplay) ||
      (StyleDisplay::Inline == parentDisplay) ||
      (StyleDisplay::InlineBlock == parentDisplay)) {
    // Recover the special style flags for the containing block
    if (containingBlock) {
      haveFirstLetterStyle = HasFirstLetterStyle(containingBlock);
      haveFirstLineStyle =
        ShouldHaveFirstLineStyle(containingBlock->GetContent(),
                                 containingBlock->StyleContext());
    }

    if (haveFirstLetterStyle) {
      // If our current insertion.mParentFrame is a Letter frame, use its parent as our
      // new parent hint
      if (insertion.mParentFrame->GetType() == nsGkAtoms::letterFrame) {
        // If insertion.mParentFrame is out of flow, then we actually want the parent of
        // the placeholder frame.
        if (insertion.mParentFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
          nsPlaceholderFrame* placeholderFrame =
            GetPlaceholderFrameFor(insertion.mParentFrame);
          NS_ASSERTION(placeholderFrame, "No placeholder for out-of-flow?");
          insertion.mParentFrame = placeholderFrame->GetParent();
        } else {
          insertion.mParentFrame = insertion.mParentFrame->GetParent();
        }
      }

      // Remove the old letter frames before doing the insertion
      RemoveLetterFrames(mPresShell, state.mFloatedItems.containingBlock);

      // Removing the letterframes messes around with the frame tree, removing
      // and creating frames.  We need to reget our prevsibling, parent frame,
      // etc.
      prevSibling = GetInsertionPrevSibling(&insertion, aStartChild, &isAppend,
                                            &isRangeInsertSafe);

      // Need check whether a range insert is still safe.
      if (!isSingleInsert && !isRangeInsertSafe) {
        // Need to recover the letter frames first.
        RecoverLetterFrames(state.mFloatedItems.containingBlock);

        // must fall back to a single ContertInserted for each child in the range
        LAYOUT_PHASE_TEMP_EXIT();
        IssueSingleInsertNofications(aContainer, aStartChild, aEndChild,
                                     aAllowLazyConstruction);
        LAYOUT_PHASE_TEMP_REENTER();
        return NS_OK;
      }

      container = insertion.mParentFrame->GetContent();
      frameType = insertion.mParentFrame->GetType();
    }
  }

  if (!prevSibling) {
    // We're inserting the new frames as the first child. See if the
    // parent has a :before pseudo-element
    nsIFrame* firstChild = insertion.mParentFrame->PrincipalChildList().FirstChild();

    if (firstChild &&
        nsLayoutUtils::IsGeneratedContentFor(container, firstChild,
                                             nsCSSPseudoElements::before)) {
      // Insert the new frames after the last continuation of the :before
      prevSibling = firstChild->GetTailContinuation();
      insertion.mParentFrame = prevSibling->GetParent()->GetContentInsertionFrame();
      // Don't change isAppend here; we'll can call AppendFrames as needed, and
      // the change to our prevSibling doesn't affect that.
    }
  }

  FrameConstructionItemList items;
  ParentType parentType = GetParentType(frameType);
  FlattenedChildIterator iter(aContainer);
  bool haveNoXBLChildren = (!iter.XBLInvolved() || !iter.GetNextChild());
  if (aStartChild->GetPreviousSibling() &&
      parentType == eTypeBlock && haveNoXBLChildren) {
    // If there's a text node in the normal content list just before the
    // new nodes, and it has no frame, make a frame construction item for
    // it, because it might need a frame now.  No need to do this if our
    // parent type is not block, though, since WipeContainingBlock
    // already handles that sitation.
    AddTextItemIfNeeded(state, insertion, aStartChild->GetPreviousSibling(),
                        items);
  }

  if (isSingleInsert) {
    AddFrameConstructionItems(state, aStartChild,
                              aStartChild->IsRootOfAnonymousSubtree(),
                              insertion, items);
  } else {
    for (nsIContent* child = aStartChild;
         child != aEndChild;
         child = child->GetNextSibling()){
      AddFrameConstructionItems(state, child, false, insertion, items);
    }
  }

  if (aEndChild && parentType == eTypeBlock && haveNoXBLChildren) {
    // If there's a text node in the normal content list just after the
    // new nodes, and it has no frame, make a frame construction item for
    // it, because it might need a frame now.  No need to do this if our
    // parent type is not block, though, since WipeContainingBlock
    // already handles that sitation.
    AddTextItemIfNeeded(state, insertion, aEndChild, items);
  }

  // Perform special check for diddling around with the frames in
  // a special inline frame.
  // If we're appending before :after content, then we're not really
  // appending, so let WipeContainingBlock know that.
  LAYOUT_PHASE_TEMP_EXIT();
  if (WipeContainingBlock(state, containingBlock, insertion.mParentFrame, items,
                          isAppend, prevSibling)) {
    LAYOUT_PHASE_TEMP_REENTER();
    return NS_OK;
  }
  LAYOUT_PHASE_TEMP_REENTER();

  // If the container is a table and a caption will be appended, it needs to be
  // put in the table wrapper frame's additional child list.
  // We make no attempt here to set flags to indicate whether the list
  // will be at the start or end of a block. It doesn't seem worthwhile.
  nsFrameItems frameItems, captionItems;
  ConstructFramesFromItemList(state, items, insertion.mParentFrame, frameItems);

  if (frameItems.NotEmpty()) {
    for (nsIContent* child = aStartChild;
         child != aEndChild;
         child = child->GetNextSibling()){
      InvalidateCanvasIfNeeded(mPresShell, child);
    }

    if (nsGkAtoms::tableFrame == frameType ||
        nsGkAtoms::tableWrapperFrame == frameType) {
      PullOutCaptionFrames(frameItems, captionItems);
    }
  }

  // If the parent of our current prevSibling is different from the frame we'll
  // actually use as the parent, then the calculated insertion point is now
  // invalid and as it is unknown where to insert correctly we append instead
  // (bug 341858).
  // This can affect our prevSibling and isAppend, but should not have any
  // effect on the WipeContainingBlock above, since this should only happen
  // when neither parent is a ib-split frame and should not affect whitespace
  // handling inside table-related frames (and in fact, can only happen when
  // one of the parents is a table wrapper and one is an inner table or when the
  // parent is a fieldset or fieldset content frame).  So it won't affect the
  // {ib} or XUL box cases in WipeContainingBlock(), and the table pseudo
  // handling will only be affected by us maybe thinking we're not inserting
  // at the beginning, whereas we really are.  That would have made us reframe
  // unnecessarily, but that's ok.
  // XXXbz we should push our frame construction item code up higher, so we
  // know what our items are by the time we start figuring out previous
  // siblings
  if (prevSibling && frameItems.NotEmpty() &&
      frameItems.FirstChild()->GetParent() != prevSibling->GetParent()) {
#ifdef DEBUG
    nsIFrame* frame1 = frameItems.FirstChild()->GetParent();
    nsIFrame* frame2 = prevSibling->GetParent();
    NS_ASSERTION(!IsFramePartOfIBSplit(frame1) &&
                 !IsFramePartOfIBSplit(frame2),
                 "Neither should be ib-split");
    NS_ASSERTION((frame1->GetType() == nsGkAtoms::tableFrame &&
                  frame2->GetType() == nsGkAtoms::tableWrapperFrame) ||
                 (frame1->GetType() == nsGkAtoms::tableWrapperFrame &&
                  frame2->GetType() == nsGkAtoms::tableFrame) ||
                 frame1->GetType() == nsGkAtoms::fieldSetFrame ||
                 (frame1->GetParent() &&
                  frame1->GetParent()->GetType() == nsGkAtoms::fieldSetFrame),
                 "Unexpected frame types");
#endif
    isAppend = true;
    nsIFrame* appendAfterFrame;
    insertion.mParentFrame =
      ::AdjustAppendParentForAfterContent(this, container,
                                          frameItems.FirstChild()->GetParent(),
                                          aStartChild, &appendAfterFrame);
    prevSibling = ::FindAppendPrevSibling(insertion.mParentFrame, appendAfterFrame);
  }

  if (haveFirstLineStyle && insertion.mParentFrame == containingBlock) {
    // It's possible that the new frame goes into a first-line
    // frame. Look at it and see...
    if (isAppend) {
      // Use append logic when appending
      AppendFirstLineFrames(state, containingBlock->GetContent(),
                            containingBlock, frameItems);
    }
    else {
      // Use more complicated insert logic when inserting
      // XXXbz this method is a no-op, so it's easy for the args being passed
      // here to make no sense without anyone noticing...  If it ever stops
      // being a no-op, vet them carefully!
      InsertFirstLineFrames(state, container, containingBlock, &insertion.mParentFrame,
                            prevSibling, frameItems);
    }
  }

  // We might have captions; put them into the caption list of the
  // table wrapper frame.
  if (captionItems.NotEmpty()) {
    NS_ASSERTION(nsGkAtoms::tableFrame == frameType ||
                 nsGkAtoms::tableWrapperFrame == frameType,
                 "parent for caption is not table?");
    // We need to determine where to put the caption items; start with the
    // the parent frame that has already been determined and get the insertion
    // prevsibling of the first caption item.
    bool captionIsAppend;
    nsIFrame* captionPrevSibling = nullptr;

    // aIsRangeInsertSafe is ignored on purpose because it is irrelevant here.
    bool ignored;
    InsertionPoint captionInsertion(insertion.mParentFrame, insertion.mContainer);
    if (isSingleInsert) {
      captionPrevSibling =
        GetInsertionPrevSibling(&captionInsertion, aStartChild,
                                &captionIsAppend, &ignored);
    } else {
      nsIContent* firstCaption = captionItems.FirstChild()->GetContent();
      // It is very important here that we skip the children in
      // [aStartChild,aEndChild) when looking for a
      // prevsibling.
      captionPrevSibling =
        GetInsertionPrevSibling(&captionInsertion, firstCaption,
                                &captionIsAppend, &ignored,
                                aStartChild, aEndChild);
    }

    nsContainerFrame* outerTable = nullptr;
    if (GetCaptionAdjustedParent(captionInsertion.mParentFrame,
                                 captionItems.FirstChild(),
                                 &outerTable)) {
      // If the parent is not a table wrapper frame we will try to add frames
      // to a named child list that the parent does not honor and the frames
      // will get lost.
      NS_ASSERTION(nsGkAtoms::tableWrapperFrame == outerTable->GetType(),
                   "Pseudo frame construction failure; "
                   "a caption can be only a child of a table wrapper frame");

      // If the parent of our current prevSibling is different from the frame
      // we'll actually use as the parent, then the calculated insertion
      // point is now invalid (bug 341382).
      if (captionPrevSibling &&
          captionPrevSibling->GetParent() != outerTable) {
          captionPrevSibling = nullptr;
      }
      if (captionIsAppend) {
        AppendFrames(outerTable, nsIFrame::kCaptionList, captionItems);
      } else {
        InsertFrames(outerTable, nsIFrame::kCaptionList,
                     captionPrevSibling, captionItems);
      }
    }
  }

  if (frameItems.NotEmpty()) {
    // Notify the parent frame
    if (isAppend) {
      AppendFramesToParent(state, insertion.mParentFrame, frameItems, prevSibling);
    } else {
      InsertFrames(insertion.mParentFrame, kPrincipalList, prevSibling, frameItems);
    }
  }

  if (haveFirstLetterStyle) {
    // Recover the letter frames for the containing block when
    // it has first-letter style.
    RecoverLetterFrames(state.mFloatedItems.containingBlock);
  }

#ifdef DEBUG
  if (gReallyNoisyContentUpdates && insertion.mParentFrame) {
    printf("nsCSSFrameConstructor::ContentRangeInserted: resulting frame model:\n");
    insertion.mParentFrame->List(stdout, 0);
  }
#endif

#ifdef ACCESSIBILITY
  nsAccessibilityService* accService = nsIPresShell::AccService();
  if (accService) {
    accService->ContentRangeInserted(mPresShell, aContainer,
                                     aStartChild, aEndChild);
  }
#endif

  return NS_OK;
}

nsresult
nsCSSFrameConstructor::ContentRemoved(nsIContent*  aContainer,
                                      nsIContent*  aChild,
                                      nsIContent*  aOldNextSibling,
                                      RemoveFlags  aFlags,
                                      bool*        aDidReconstruct,
                                      nsIContent** aDestroyedFramesFor)
{
  AUTO_LAYOUT_PHASE_ENTRY_POINT(mPresShell->GetPresContext(), FrameC);
  NS_PRECONDITION(mUpdateCount != 0,
                  "Should be in an update while destroying frames");

  *aDidReconstruct = false;
  if (aDestroyedFramesFor) {
    *aDestroyedFramesFor = aChild;
  }

  if (aChild->IsHTMLElement(nsGkAtoms::body) ||
      (!aContainer && aChild->IsElement())) {
    // This might be the element we propagated viewport scrollbar
    // styles from.  Recompute those.
    mPresShell->GetPresContext()->UpdateViewportScrollbarStylesOverride();
  }

  // XXXldb Do we need to re-resolve style to handle the CSS2 + combinator and
  // the :empty pseudo-class?

#ifdef DEBUG
  if (gNoisyContentUpdates) {
    printf("nsCSSFrameConstructor::ContentRemoved container=%p child=%p "
           "old-next-sibling=%p\n",
           static_cast<void*>(aContainer),
           static_cast<void*>(aChild),
           static_cast<void*>(aOldNextSibling));
    if (gReallyNoisyContentUpdates) {
      aContainer->List(stdout, 0);
    }
  }
#endif

  nsresult rv = NS_OK;
  nsIFrame* childFrame = aChild->GetPrimaryFrame();
  if (!childFrame || childFrame->GetContent() != aChild) {
    // XXXbz the GetContent() != aChild check is needed due to bug 135040.
    // Remove it once that's fixed.
    ClearUndisplayedContentIn(aChild, aContainer);
  }
  MOZ_ASSERT(!childFrame || !GetDisplayContentsStyleFor(aChild),
             "display:contents nodes shouldn't have a frame");
  if (!childFrame && GetDisplayContentsStyleFor(aChild)) {
    nsIFrame* ancestorFrame = nullptr;
    nsIContent* ancestor = aContainer;
    for (; ancestor; ancestor = ancestor->GetParent()) {
      ancestorFrame = ancestor->GetPrimaryFrame();
      if (ancestorFrame) {
        break;
      }
    }
    if (ancestorFrame) {
      nsTArray<nsIContent*>* generated = ancestorFrame->GetGenConPseudos();
      if (generated) {
        *aDidReconstruct = true;
        LAYOUT_PHASE_TEMP_EXIT();
        // XXXmats Can we recreate frames only for the ::after/::before content?
        // XXX Perhaps even only those that belong to the aChild sub-tree?
        RecreateFramesForContent(ancestor, false, aFlags, aDestroyedFramesFor);
        LAYOUT_PHASE_TEMP_REENTER();
        return NS_OK;
      }
    }

    FlattenedChildIterator iter(aChild);
    for (nsIContent* c = iter.GetNextChild(); c; c = iter.GetNextChild()) {
      if (c->GetPrimaryFrame() || GetDisplayContentsStyleFor(c)) {
        LAYOUT_PHASE_TEMP_EXIT();
        rv = ContentRemoved(aChild, c, nullptr, aFlags, aDidReconstruct, aDestroyedFramesFor);
        LAYOUT_PHASE_TEMP_REENTER();
        NS_ENSURE_SUCCESS(rv, rv);
        if (aFlags != REMOVE_DESTROY_FRAMES && *aDidReconstruct) {
          return rv;
        }
      }
    }
    ClearDisplayContentsIn(aChild, aContainer);
  }

  nsPresContext* presContext = mPresShell->GetPresContext();
#ifdef MOZ_XUL
  if (NotifyListBoxBody(presContext, aContainer, aChild, aOldNextSibling,
                        childFrame, CONTENT_REMOVED)) {
    if (aFlags == REMOVE_DESTROY_FRAMES) {
      CaptureStateForFramesOf(aChild, mTempFrameTreeState);
    }
    return NS_OK;
  }

#endif // MOZ_XUL

  // If we're removing the root, then make sure to remove things starting at
  // the viewport's child instead of the primary frame (which might even be
  // null if the root had an XBL binding or display:none, even though the
  // frames above it got created).  We do the adjustment after the childFrame
  // check above, because we do want to clear any undisplayed content we might
  // have for the root.  Detecting removal of a root is a little exciting; in
  // particular, having a null aContainer is necessary but NOT sufficient.  Due
  // to how we process reframes, the content node might not even be in our
  // document by now.  So explicitly check whether the viewport's first kid's
  // content node is aChild.
  bool isRoot = false;
  if (!aContainer) {
    nsIFrame* viewport = GetRootFrame();
    if (viewport) {
      nsIFrame* firstChild = viewport->PrincipalChildList().FirstChild();
      if (firstChild && firstChild->GetContent() == aChild) {
        isRoot = true;
        childFrame = firstChild;
        NS_ASSERTION(!childFrame->GetNextSibling(), "How did that happen?");
      }
    }
  }

  if (aContainer && aContainer->HasFlag(NODE_IS_IN_SHADOW_TREE) &&
      !aContainer->IsInNativeAnonymousSubtree() &&
      !aChild->IsInNativeAnonymousSubtree()) {
    // Recreate frames if content is removed from a ShadowRoot
    // because it may contain an insertion point which can change
    // how the host is rendered.
    //XXXsmaug This is super unefficient!
    nsIContent* bindingParent = aContainer->GetBindingParent();
    *aDidReconstruct = true;
    LAYOUT_PHASE_TEMP_EXIT();
    nsresult rv = RecreateFramesForContent(bindingParent, false,
                                           aFlags, aDestroyedFramesFor);
    LAYOUT_PHASE_TEMP_REENTER();
    return rv;
  }

  if (aFlags == REMOVE_DESTROY_FRAMES) {
    CaptureStateForFramesOf(aChild, mTempFrameTreeState);
  }

  if (childFrame) {
    InvalidateCanvasIfNeeded(mPresShell, aChild);

    // See whether we need to remove more than just childFrame
    LAYOUT_PHASE_TEMP_EXIT();
    nsIContent* container;
    if (MaybeRecreateContainerForFrameRemoval(childFrame, aFlags, &rv, &container)) {
      LAYOUT_PHASE_TEMP_REENTER();
      MOZ_ASSERT(container);
      *aDidReconstruct = true;
      if (aDestroyedFramesFor) {
        *aDestroyedFramesFor = container;
      }
      return rv;
    }
    LAYOUT_PHASE_TEMP_REENTER();

    // Get the childFrame's parent frame
    nsIFrame* parentFrame = childFrame->GetParent();
    nsIAtom* parentType = parentFrame->GetType();

    if (parentType == nsGkAtoms::frameSetFrame &&
        IsSpecialFramesetChild(aChild)) {
      // Just reframe the parent, since framesets are weird like that.
      *aDidReconstruct = true;
      LAYOUT_PHASE_TEMP_EXIT();
      nsresult rv = RecreateFramesForContent(parentFrame->GetContent(), false,
                                             aFlags, aDestroyedFramesFor);
      LAYOUT_PHASE_TEMP_REENTER();
      return rv;
    }

    // If we're a child of MathML, then we should reframe the MathML content.
    // If we're non-MathML, then we would be wrapped in a block so we need to
    // check our grandparent in that case.
    nsIFrame* possibleMathMLAncestor = parentType == nsGkAtoms::blockFrame ?
         parentFrame->GetParent() : parentFrame;
    if (possibleMathMLAncestor->IsFrameOfType(nsIFrame::eMathML)) {
      *aDidReconstruct = true;
      LAYOUT_PHASE_TEMP_EXIT();
      nsresult rv = RecreateFramesForContent(possibleMathMLAncestor->GetContent(),
                                             false, aFlags, aDestroyedFramesFor);
      LAYOUT_PHASE_TEMP_REENTER();
      return rv;
    }

    // Undo XUL wrapping if it's no longer needed.
    // (If we're in the XUL block-wrapping situation, parentFrame is the
    // wrapper frame.)
    nsIFrame* grandparentFrame = parentFrame->GetParent();
    if (grandparentFrame && grandparentFrame->IsXULBoxFrame() &&
        (grandparentFrame->GetStateBits() & NS_STATE_BOX_WRAPS_KIDS_IN_BLOCK) &&
        // check if this frame is the only one needing wrapping
        aChild == AnyKidsNeedBlockParent(parentFrame->PrincipalChildList().FirstChild()) &&
        !AnyKidsNeedBlockParent(childFrame->GetNextSibling())) {
      *aDidReconstruct = true;
      LAYOUT_PHASE_TEMP_EXIT();
      nsresult rv = RecreateFramesForContent(grandparentFrame->GetContent(), true,
                                             aFlags, aDestroyedFramesFor);
      LAYOUT_PHASE_TEMP_REENTER();
      return rv;
    }

#ifdef ACCESSIBILITY
    nsAccessibilityService* accService = nsIPresShell::AccService();
    if (accService) {
      accService->ContentRemoved(mPresShell, aChild);
    }
#endif

    // Examine the containing-block for the removed content and see if
    // :first-letter style applies.
    nsIFrame* inflowChild = childFrame;
    if (childFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
      inflowChild = GetPlaceholderFrameFor(childFrame);
      NS_ASSERTION(inflowChild, "No placeholder for out-of-flow?");
    }
    nsContainerFrame* containingBlock =
      GetFloatContainingBlock(inflowChild->GetParent());
    bool haveFLS = containingBlock && HasFirstLetterStyle(containingBlock);
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
      RemoveLetterFrames(mPresShell, containingBlock);

      // Recover childFrame and parentFrame
      childFrame = aChild->GetPrimaryFrame();
      if (!childFrame || childFrame->GetContent() != aChild) {
        // XXXbz the GetContent() != aChild check is needed due to bug 135040.
        // Remove it once that's fixed.
        ClearUndisplayedContentIn(aChild, aContainer);
        return NS_OK;
      }
      parentFrame = childFrame->GetParent();
      parentType = parentFrame->GetType();

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
      putchar('\n');
      parentFrame->List(stdout, 0);
    }
#endif


    // Notify the parent frame that it should delete the frame
    if (childFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
      childFrame = GetPlaceholderFrameFor(childFrame);
      NS_ASSERTION(childFrame, "Missing placeholder frame for out of flow.");
      parentFrame = childFrame->GetParent();
    }
    RemoveFrame(nsLayoutUtils::GetChildListNameFor(childFrame), childFrame);

    if (isRoot) {
      mRootElementFrame = nullptr;
      mRootElementStyleFrame = nullptr;
      mDocElementContainingBlock = nullptr;
      mPageSequenceFrame = nullptr;
      mGfxScrollFrame = nullptr;
      mHasRootAbsPosContainingBlock = false;
    }

    if (haveFLS && mRootElementFrame) {
      RecoverLetterFrames(containingBlock);
    }

    // If we're just reconstructing frames for the element, then the
    // following ContentInserted notification on the element will
    // take care of fixing up any adjacent text nodes.  We don't need
    // to do this if the table parent type of our parent type is not
    // eTypeBlock, though, because in that case the whitespace isn't
    // being suppressed due to us anyway.
    if (aContainer && !aChild->IsRootOfAnonymousSubtree() &&
        aFlags == REMOVE_CONTENT &&
        GetParentType(parentType) == eTypeBlock) {
      // Adjacent whitespace-only text nodes might have been suppressed if
      // this node does not have inline ends. Create frames for them now
      // if necessary.
      // Reframe any text node just before the node being removed, if there is
      // one, and if it's not the last child or the first child. If a whitespace
      // textframe was being suppressed and it's now the last child or first
      // child then it can stay suppressed since the parent must be a block
      // and hence it's adjacent to a block end.
      // If aOldNextSibling is null, then the text node before the node being
      // removed is the last node, and we don't need to worry about it.
      if (aOldNextSibling) {
        nsIContent* prevSibling = aOldNextSibling->GetPreviousSibling();
        if (prevSibling && prevSibling->GetPreviousSibling()) {
          LAYOUT_PHASE_TEMP_EXIT();
          ReframeTextIfNeeded(aContainer, prevSibling);
          LAYOUT_PHASE_TEMP_REENTER();
        }
      }
      // Reframe any text node just after the node being removed, if there is
      // one, and if it's not the last child or the first child.
      if (aOldNextSibling && aOldNextSibling->GetNextSibling() &&
          aOldNextSibling->GetPreviousSibling()) {
        LAYOUT_PHASE_TEMP_EXIT();
        ReframeTextIfNeeded(aContainer, aOldNextSibling);
        LAYOUT_PHASE_TEMP_REENTER();
      }
    }

#ifdef DEBUG
    if (gReallyNoisyContentUpdates && parentFrame) {
      printf("nsCSSFrameConstructor::ContentRemoved: resulting frame model:\n");
      parentFrame->List(stdout, 0);
    }
#endif
  }

  return rv;
}

/**
 * This method invalidates the canvas when frames are removed or added for a
 * node that might have its background propagated to the canvas, i.e., a
 * document root node or an HTML BODY which is a child of the root node.
 *
 * @param aFrame a frame for a content node about to be removed or a frame that
 *               was just created for a content node that was inserted.
 */
static void
InvalidateCanvasIfNeeded(nsIPresShell* presShell, nsIContent* node)
{
  NS_PRECONDITION(presShell->GetRootFrame(), "What happened here?");
  NS_PRECONDITION(presShell->GetPresContext(), "Say what?");

  //  Note that both in ContentRemoved and ContentInserted the content node
  //  will still have the right parent pointer, so looking at that is ok.

  nsIContent* parent = node->GetParent();
  if (parent) {
    // Has a parent; might not be what we want
    nsIContent* grandParent = parent->GetParent();
    if (grandParent) {
      // Has a grandparent, so not what we want
      return;
    }

    // Check whether it's an HTML body
    if (!node->IsHTMLElement(nsGkAtoms::body)) {
      return;
    }
  }

  // At this point the node has no parent or it's an HTML <body> child of the
  // root.  We might not need to invalidate in this case (eg we might be in
  // XHTML or something), but chances are we want to.  Play it safe.
  // Invalidate the viewport.

  nsIFrame* rootFrame = presShell->GetRootFrame();
  rootFrame->InvalidateFrameSubtree();
}

nsIFrame*
nsCSSFrameConstructor::EnsureFrameForTextNode(nsGenericDOMDataNode* aContent)
{
  if (aContent->HasFlag(NS_CREATE_FRAME_IF_NON_WHITESPACE) &&
      !mAlwaysCreateFramesForIgnorableWhitespace) {
    // Text frame may have been suppressed. Disable suppression and signal
    // that a flush should be performed. We do this on a document-wide
    // basis so that pages that repeatedly query metrics for
    // collapsed-whitespace text nodes don't trigger pathological behavior.
    mAlwaysCreateFramesForIgnorableWhitespace = true;
    nsAutoScriptBlocker blocker;
    BeginUpdate();
    ReconstructDocElementHierarchy();
    EndUpdate();
  }
  return aContent->GetPrimaryFrame();
}

nsresult
nsCSSFrameConstructor::CharacterDataChanged(nsIContent* aContent,
                                            CharacterDataChangeInfo* aInfo)
{
  AUTO_LAYOUT_PHASE_ENTRY_POINT(mPresShell->GetPresContext(), FrameC);
  nsresult      rv = NS_OK;

  if ((aContent->HasFlag(NS_CREATE_FRAME_IF_NON_WHITESPACE) &&
       !aContent->TextIsOnlyWhitespace()) ||
      (aContent->HasFlag(NS_REFRAME_IF_WHITESPACE) &&
       aContent->TextIsOnlyWhitespace())) {
#ifdef DEBUG
    nsIFrame* frame = aContent->GetPrimaryFrame();
    NS_ASSERTION(!frame || !frame->IsGeneratedContentFrame(),
                 "Bit should never be set on generated content");
#endif
    LAYOUT_PHASE_TEMP_EXIT();
    nsresult rv = RecreateFramesForContent(aContent, false,
                                           REMOVE_FOR_RECONSTRUCTION, nullptr);
    LAYOUT_PHASE_TEMP_REENTER();
    return rv;
  }

  // Find the child frame
  nsIFrame* frame = aContent->GetPrimaryFrame();

  // Notify the first frame that maps the content. It will generate a reflow
  // command

  // It's possible the frame whose content changed isn't inserted into the
  // frame hierarchy yet, or that there is no frame that maps the content
  if (nullptr != frame) {
#if 0
    NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
       ("nsCSSFrameConstructor::CharacterDataChanged: content=%p[%s] subcontent=%p frame=%p",
        aContent, ContentTag(aContent, 0),
        aSubContent, frame));
#endif

    // Special check for text content that is a child of a letter frame.  If
    // this happens, we should remove the letter frame, do whatever we're
    // planning to do with this notification, then put the letter frame back.
    // Note that this is basically what RecreateFramesForContent ends up doing;
    // the reason we dont' want to call that here is that our text content
    // could be native anonymous, in which case RecreateFramesForContent would
    // completely barf on it.  And recreating the non-anonymous ancestor would
    // just lead us to come back into this notification (e.g. if quotes or
    // counters are involved), leading to a loop.
    nsContainerFrame* block = GetFloatContainingBlock(frame);
    bool haveFirstLetterStyle = false;
    if (block) {
      // See if the block has first-letter style applied to it.
      haveFirstLetterStyle = HasFirstLetterStyle(block);
      if (haveFirstLetterStyle) {
        RemoveLetterFrames(mPresShell, block);
        // Reget |frame|, since we might have killed it.
        // Do we really need to call CharacterDataChanged in this case, though?
        frame = aContent->GetPrimaryFrame();
        NS_ASSERTION(frame, "Should have frame here!");
      }
    }

    frame->CharacterDataChanged(aInfo);

    if (haveFirstLetterStyle) {
      RecoverLetterFrames(block);
    }
  }

  return rv;
}

void
nsCSSFrameConstructor::BeginUpdate() {
  NS_ASSERTION(!nsContentUtils::IsSafeToRunScript(),
               "Someone forgot a script blocker");

  nsRootPresContext* rootPresContext =
    mPresShell->GetPresContext()->GetRootPresContext();
  if (rootPresContext) {
    rootPresContext->IncrementDOMGeneration();
  }

  ++sGlobalGenerationNumber;
#ifdef DEBUG
  ++mUpdateCount;
#endif
}

void
nsCSSFrameConstructor::EndUpdate()
{
#ifdef DEBUG
  NS_ASSERTION(mUpdateCount, "Negative mUpdateCount!");
  --mUpdateCount;
#endif
}

void
nsCSSFrameConstructor::RecalcQuotesAndCounters()
{
  if (mQuotesDirty) {
    mQuotesDirty = false;
    mQuoteList.RecalcAll();
  }

  if (mCountersDirty) {
    mCountersDirty = false;
    mCounterManager.RecalcAll();
  }

  NS_ASSERTION(!mQuotesDirty, "Quotes updates will be lost");
  NS_ASSERTION(!mCountersDirty, "Counter updates will be lost");
}

void
nsCSSFrameConstructor::NotifyCounterStylesAreDirty()
{
  NS_PRECONDITION(mUpdateCount != 0, "Should be in an update");
  mCounterManager.SetAllCounterStylesDirty();
  CountersDirty();
}

void
nsCSSFrameConstructor::WillDestroyFrameTree()
{
#if defined(DEBUG_dbaron_off)
  mCounterManager.Dump();
#endif

  mIsDestroyingFrameTree = true;

  // Prevent frame tree destruction from being O(N^2)
  mQuoteList.Clear();
  mCounterManager.Clear();

  // Remove our presshell as a style flush observer.  But leave
  // RestyleManager::mObservingRefreshDriver true so we don't readd to
  // it even if someone tries to post restyle events on us from this
  // point on for some reason.
  mPresShell->GetPresContext()->RefreshDriver()->
    RemoveStyleFlushObserver(mPresShell);

  nsFrameManager::Destroy();
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

nsIFrame*
nsCSSFrameConstructor::CreateContinuingOuterTableFrame(nsIPresShell*     aPresShell,
                                                       nsPresContext*    aPresContext,
                                                       nsIFrame*         aFrame,
                                                       nsContainerFrame* aParentFrame,
                                                       nsIContent*       aContent,
                                                       nsStyleContext*   aStyleContext)
{
  nsTableWrapperFrame* newFrame = NS_NewTableWrapperFrame(aPresShell, aStyleContext);

  newFrame->Init(aContent, aParentFrame, aFrame);

  // Create a continuing inner table frame, and if there's a caption then
  // replicate the caption
  nsFrameItems  newChildFrames;

  nsIFrame* childFrame = aFrame->PrincipalChildList().FirstChild();
  if (childFrame) {
    nsIFrame* continuingTableFrame =
      CreateContinuingFrame(aPresContext, childFrame, newFrame);
    newChildFrames.AddChild(continuingTableFrame);

    NS_ASSERTION(!childFrame->GetNextSibling(),"there can be only one inner table frame");
  }

  // Set the table wrapper's initial child list
  newFrame->SetInitialChildList(kPrincipalList, newChildFrames);

  return newFrame;
}

nsIFrame*
nsCSSFrameConstructor::CreateContinuingTableFrame(nsIPresShell*     aPresShell,
                                                  nsIFrame*         aFrame,
                                                  nsContainerFrame* aParentFrame,
                                                  nsIContent*       aContent,
                                                  nsStyleContext*   aStyleContext)
{
  nsTableFrame* newFrame = NS_NewTableFrame(aPresShell, aStyleContext);

  newFrame->Init(aContent, aParentFrame, aFrame);

  // Replicate any header/footer frames
  nsFrameItems  childFrames;
  for (nsIFrame* childFrame : aFrame->PrincipalChildList()) {
    // See if it's a header/footer, possibly wrapped in a scroll frame.
    nsTableRowGroupFrame* rowGroupFrame =
      static_cast<nsTableRowGroupFrame*>(childFrame);
    // If the row group was continued, then don't replicate it.
    nsIFrame* rgNextInFlow = rowGroupFrame->GetNextInFlow();
    if (rgNextInFlow) {
      rowGroupFrame->SetRepeatable(false);
    }
    else if (rowGroupFrame->IsRepeatable()) {
      // Replicate the header/footer frame.
      nsTableRowGroupFrame*   headerFooterFrame;
      nsFrameItems            childItems;
      nsFrameConstructorState state(mPresShell,
                                    GetAbsoluteContainingBlock(newFrame, FIXED_POS),
                                    GetAbsoluteContainingBlock(newFrame, ABS_POS),
                                    nullptr);
      state.mCreatingExtraFrames = true;

      nsStyleContext* const headerFooterStyleContext = rowGroupFrame->StyleContext();
      headerFooterFrame = static_cast<nsTableRowGroupFrame*>
                                     (NS_NewTableRowGroupFrame(aPresShell, headerFooterStyleContext));

      nsIContent* headerFooter = rowGroupFrame->GetContent();
      headerFooterFrame->Init(headerFooter, newFrame, nullptr);

      nsFrameConstructorSaveState absoluteSaveState;
      MakeTablePartAbsoluteContainingBlockIfNeeded(state,
                                                   headerFooterStyleContext->StyleDisplay(),
                                                   absoluteSaveState,
                                                   headerFooterFrame);

      ProcessChildren(state, headerFooter, rowGroupFrame->StyleContext(),
                      headerFooterFrame, true, childItems, false,
                      nullptr);
      NS_ASSERTION(state.mFloatedItems.IsEmpty(), "unexpected floated element");
      headerFooterFrame->SetInitialChildList(kPrincipalList, childItems);
      headerFooterFrame->SetRepeatable(true);

      // Table specific initialization
      headerFooterFrame->InitRepeatedFrame(rowGroupFrame);

      // XXX Deal with absolute and fixed frames...
      childFrames.AddChild(headerFooterFrame);
    }
  }

  // Set the table frame's initial child list
  newFrame->SetInitialChildList(kPrincipalList, childFrames);

  return newFrame;
}

nsIFrame*
nsCSSFrameConstructor::CreateContinuingFrame(nsPresContext*    aPresContext,
                                             nsIFrame*         aFrame,
                                             nsContainerFrame* aParentFrame,
                                             bool              aIsFluid)
{
  nsIPresShell*              shell = aPresContext->PresShell();
  nsStyleContext*            styleContext = aFrame->StyleContext();
  nsIFrame*                  newFrame = nullptr;
  nsIFrame*                  nextContinuation = aFrame->GetNextContinuation();
  nsIFrame*                  nextInFlow = aFrame->GetNextInFlow();

  // Use the frame type to determine what type of frame to create
  nsIAtom* frameType = aFrame->GetType();
  nsIContent* content = aFrame->GetContent();

  NS_ASSERTION(aFrame->GetSplittableType() != NS_FRAME_NOT_SPLITTABLE,
               "why CreateContinuingFrame for a non-splittable frame?");

  if (nsGkAtoms::textFrame == frameType) {
    newFrame = NS_NewContinuingTextFrame(shell, styleContext);
    newFrame->Init(content, aParentFrame, aFrame);
  } else if (nsGkAtoms::inlineFrame == frameType) {
    newFrame = NS_NewInlineFrame(shell, styleContext);
    newFrame->Init(content, aParentFrame, aFrame);
  } else if (nsGkAtoms::blockFrame == frameType) {
    MOZ_ASSERT(!aFrame->IsTableCaption(),
               "no support for fragmenting table captions yet");
    newFrame = NS_NewBlockFrame(shell, styleContext);
    newFrame->Init(content, aParentFrame, aFrame);
#ifdef MOZ_XUL
  } else if (nsGkAtoms::XULLabelFrame == frameType) {
    newFrame = NS_NewXULLabelFrame(shell, styleContext);
    newFrame->Init(content, aParentFrame, aFrame);
#endif
  } else if (nsGkAtoms::columnSetFrame == frameType) {
    MOZ_ASSERT(!aFrame->IsTableCaption(),
               "no support for fragmenting table captions yet");
    newFrame = NS_NewColumnSetFrame(shell, styleContext, nsFrameState(0));
    newFrame->Init(content, aParentFrame, aFrame);
  } else if (nsGkAtoms::pageFrame == frameType) {
    nsContainerFrame* canvasFrame;
    newFrame = ConstructPageFrame(shell, aParentFrame, aFrame, canvasFrame);
  } else if (nsGkAtoms::tableWrapperFrame == frameType) {
    newFrame =
      CreateContinuingOuterTableFrame(shell, aPresContext, aFrame, aParentFrame,
                                      content, styleContext);

  } else if (nsGkAtoms::tableFrame == frameType) {
    newFrame =
      CreateContinuingTableFrame(shell, aFrame, aParentFrame,
                                 content, styleContext);

  } else if (nsGkAtoms::tableRowGroupFrame == frameType) {
    newFrame = NS_NewTableRowGroupFrame(shell, styleContext);
    newFrame->Init(content, aParentFrame, aFrame);
    if (newFrame->GetStateBits() & NS_FRAME_CAN_HAVE_ABSPOS_CHILDREN) {
      nsTableFrame::RegisterPositionedTablePart(newFrame);
    }
  } else if (nsGkAtoms::tableRowFrame == frameType) {
    nsTableRowFrame* rowFrame = NS_NewTableRowFrame(shell, styleContext);

    rowFrame->Init(content, aParentFrame, aFrame);
    if (rowFrame->GetStateBits() & NS_FRAME_CAN_HAVE_ABSPOS_CHILDREN) {
      nsTableFrame::RegisterPositionedTablePart(rowFrame);
    }

    // Create a continuing frame for each table cell frame
    nsFrameItems  newChildList;
    nsIFrame* cellFrame = aFrame->PrincipalChildList().FirstChild();
    while (cellFrame) {
      // See if it's a table cell frame
      if (IS_TABLE_CELL(cellFrame->GetType())) {
        nsIFrame* continuingCellFrame =
          CreateContinuingFrame(aPresContext, cellFrame, rowFrame);
        newChildList.AddChild(continuingCellFrame);
      }
      cellFrame = cellFrame->GetNextSibling();
    }

    rowFrame->SetInitialChildList(kPrincipalList, newChildList);
    newFrame = rowFrame;

  } else if (IS_TABLE_CELL(frameType)) {
    // Warning: If you change this and add a wrapper frame around table cell
    // frames, make sure Bug 368554 doesn't regress!
    // See IsInAutoWidthTableCellForQuirk() in nsImageFrame.cpp.
    nsTableFrame* tableFrame =
      static_cast<nsTableRowFrame*>(aParentFrame)->GetTableFrame();
    nsTableCellFrame* cellFrame =
      NS_NewTableCellFrame(shell, styleContext, tableFrame);

    cellFrame->Init(content, aParentFrame, aFrame);
    if (cellFrame->GetStateBits() & NS_FRAME_CAN_HAVE_ABSPOS_CHILDREN) {
      nsTableFrame::RegisterPositionedTablePart(cellFrame);
    }

    // Create a continuing area frame
    nsIFrame* blockFrame = aFrame->PrincipalChildList().FirstChild();
    nsIFrame* continuingBlockFrame =
      CreateContinuingFrame(aPresContext, blockFrame,
                            static_cast<nsContainerFrame*>(cellFrame));

    SetInitialSingleChild(cellFrame, continuingBlockFrame);
    newFrame = cellFrame;
  } else if (nsGkAtoms::lineFrame == frameType) {
    newFrame = NS_NewFirstLineFrame(shell, styleContext);
    newFrame->Init(content, aParentFrame, aFrame);
  } else if (nsGkAtoms::letterFrame == frameType) {
    newFrame = NS_NewFirstLetterFrame(shell, styleContext);
    newFrame->Init(content, aParentFrame, aFrame);
  } else if (nsGkAtoms::imageFrame == frameType) {
    newFrame = NS_NewImageFrame(shell, styleContext);
    newFrame->Init(content, aParentFrame, aFrame);
  } else if (nsGkAtoms::imageControlFrame == frameType) {
    newFrame = NS_NewImageControlFrame(shell, styleContext);
    newFrame->Init(content, aParentFrame, aFrame);
  } else if (nsGkAtoms::placeholderFrame == frameType) {
    // create a continuing out of flow frame
    nsIFrame* oofFrame = nsPlaceholderFrame::GetRealFrameForPlaceholder(aFrame);
    nsIFrame* oofContFrame =
      CreateContinuingFrame(aPresContext, oofFrame, aParentFrame);
    newFrame =
      CreatePlaceholderFrameFor(shell, content, oofContFrame,
                                styleContext->GetParent(),
                                aParentFrame, aFrame,
                                aFrame->GetStateBits() & PLACEHOLDER_TYPE_MASK);
  } else if (nsGkAtoms::fieldSetFrame == frameType) {
    nsContainerFrame* fieldset = NS_NewFieldSetFrame(shell, styleContext);

    fieldset->Init(content, aParentFrame, aFrame);

    // Create a continuing area frame
    // XXXbz we really shouldn't have to do this by hand!
    nsContainerFrame* blockFrame = GetFieldSetBlockFrame(aFrame);
    if (blockFrame) {
      nsIFrame* continuingBlockFrame =
        CreateContinuingFrame(aPresContext, blockFrame, fieldset);
      // Set the fieldset's initial child list
      SetInitialSingleChild(fieldset, continuingBlockFrame);
    } else {
      MOZ_ASSERT(aFrame->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER,
                 "FieldSet block may only be null for overflow containers");
    }
    newFrame = fieldset;
  } else if (nsGkAtoms::legendFrame == frameType) {
    newFrame = NS_NewLegendFrame(shell, styleContext);
    newFrame->Init(content, aParentFrame, aFrame);
  } else if (nsGkAtoms::flexContainerFrame == frameType) {
    newFrame = NS_NewFlexContainerFrame(shell, styleContext);
    newFrame->Init(content, aParentFrame, aFrame);
  } else if (nsGkAtoms::gridContainerFrame == frameType) {
    newFrame = NS_NewGridContainerFrame(shell, styleContext);
    newFrame->Init(content, aParentFrame, aFrame);
  } else if (nsGkAtoms::rubyFrame == frameType) {
    newFrame = NS_NewRubyFrame(shell, styleContext);
    newFrame->Init(content, aParentFrame, aFrame);
  } else if (nsGkAtoms::rubyBaseContainerFrame == frameType) {
    newFrame = NS_NewRubyBaseContainerFrame(shell, styleContext);
    newFrame->Init(content, aParentFrame, aFrame);
  } else if (nsGkAtoms::rubyTextContainerFrame == frameType) {
    newFrame = NS_NewRubyTextContainerFrame(shell, styleContext);
    newFrame->Init(content, aParentFrame, aFrame);
  } else if (nsGkAtoms::detailsFrame == frameType) {
    newFrame = NS_NewDetailsFrame(shell, styleContext);
    newFrame->Init(content, aParentFrame, aFrame);
  } else {
    MOZ_CRASH("unexpected frame type");
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

  // A continuation of nsIAnonymousContentCreator content is also
  // nsIAnonymousContentCreator created content
  if (aFrame->GetStateBits() & NS_FRAME_ANONYMOUSCONTENTCREATOR_CONTENT) {
    newFrame->AddStateBits(NS_FRAME_ANONYMOUSCONTENTCREATOR_CONTENT);
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

  NS_POSTCONDITION(!newFrame->GetNextSibling(), "unexpected sibling");
  return newFrame;
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
  nsContainerFrame* canvasFrame =
    do_QueryFrame(aParentFrame->PrincipalChildList().FirstChild());
  nsIFrame* prevCanvasFrame = prevPageContentFrame->PrincipalChildList().FirstChild();
  if (!canvasFrame || !prevCanvasFrame) {
    // document's root element frame missing
    return NS_ERROR_UNEXPECTED;
  }

  nsFrameItems fixedPlaceholders;
  nsIFrame* firstFixed = prevPageContentFrame->GetChildList(nsIFrame::kFixedList).FirstChild();
  if (!firstFixed) {
    return NS_OK;
  }

  // Don't allow abs-pos descendants of the fixed content to escape the content.
  // This should not normally be possible (because fixed-pos elements should
  // be absolute containers) but fixed-pos tables currently aren't abs-pos
  // containers.
  nsFrameConstructorState state(mPresShell, aParentFrame,
                                nullptr,
                                mRootElementFrame);
  state.mCreatingExtraFrames = true;

  // We can't use an ancestor filter here, because we're not going to
  // be usefully recurring down the tree.  This means that other
  // places in frame construction can't assume a filter is
  // initialized!

  // Iterate across fixed frames and replicate each whose placeholder is a
  // descendant of aFrame. (We don't want to explicitly copy placeholders that
  // are within fixed frames, because that would cause duplicates on the new
  // page - bug 389619)
  for (nsIFrame* fixed = firstFixed; fixed; fixed = fixed->GetNextSibling()) {
    nsIFrame* prevPlaceholder = GetPlaceholderFrameFor(fixed);
    if (prevPlaceholder &&
        nsLayoutUtils::IsProperAncestorFrame(prevCanvasFrame, prevPlaceholder)) {
      // We want to use the same style as the primary style frame for
      // our content
      nsIContent* content = fixed->GetContent();
      nsStyleContext* styleContext =
        nsLayoutUtils::GetStyleFrame(content)->StyleContext();
      FrameConstructionItemList items;
      AddFrameConstructionItemsInternal(state, content, canvasFrame,
                                        content->NodeInfo()->NameAtom(),
                                        content->GetNameSpaceID(),
                                        true,
                                        styleContext,
                                        ITEM_ALLOW_XBL_BASE |
                                          ITEM_ALLOW_PAGE_BREAK,
                                        nullptr, items);
      ConstructFramesFromItemList(state, items, canvasFrame, fixedPlaceholders);
    }
  }

  // Add the placeholders to our primary child list.
  // XXXbz this is a little screwed up, since the fixed frames will have
  // broken auto-positioning. Oh, well.
  NS_ASSERTION(!canvasFrame->PrincipalChildList().FirstChild(),
               "leaking frames; doc root continuation must be empty");
  canvasFrame->SetInitialChildList(kPrincipalList, fixedPlaceholders);
  return NS_OK;
}

nsCSSFrameConstructor::InsertionPoint
nsCSSFrameConstructor::GetInsertionPoint(nsIContent* aContainer,
                                         nsIContent* aChild)
{
  nsBindingManager* bindingManager = mDocument->BindingManager();

  nsIContent* insertionElement;
  if (aChild) {
    // We've got an explicit insertion child. Check to see if it's
    // anonymous.
    if (aChild->GetBindingParent() == aContainer) {
      // This child content is anonymous. Don't use the insertion
      // point, since that's only for the explicit kids.
      return InsertionPoint(GetContentInsertionFrameFor(aContainer), aContainer);
    }

    if (nsContentUtils::HasDistributedChildren(aContainer)) {
      // The container distributes nodes, use the frame of the flattened tree parent.
      // It may be the case that the node is distributed but not matched to any
      // insertion points, so there is no flattened parent.
      nsIContent* flattenedParent = aChild->GetFlattenedTreeParent();
      if (flattenedParent) {
        return InsertionPoint(GetContentInsertionFrameFor(flattenedParent),
                              flattenedParent);
      }
      return InsertionPoint();
    }

    insertionElement = bindingManager->FindNestedInsertionPoint(aContainer, aChild);
  } else {
    if (nsContentUtils::HasDistributedChildren(aContainer)) {
      // The container distributes nodes to shadow DOM insertion points.
      // Return with aMultiple set to true to induce callers to insert children
      // individually into the node's flattened tree parent.
      return InsertionPoint(nullptr, nullptr, true);
    }

    bool multiple;
    insertionElement = bindingManager->FindNestedSingleInsertionPoint(aContainer, &multiple);
    if (multiple) {
      return InsertionPoint(nullptr, nullptr, true);
    }
  }

  if (!insertionElement) {
    insertionElement = aContainer;
  }
  InsertionPoint insertion(GetContentInsertionFrameFor(insertionElement),
                           insertionElement);

  // Fieldset frames have multiple normal flow child frame lists so handle it
  // the same as if it had multiple content insertion points.
  if (insertion.mParentFrame &&
      insertion.mParentFrame->GetType() == nsGkAtoms::fieldSetFrame) {
    insertion.mMultiple = true;
  }

  // A details frame moves the first summary frame to be its first child, so we
  // treat it as if it has multiple content insertion points.
  if (insertion.mParentFrame &&
      insertion.mParentFrame->GetType() == nsGkAtoms::detailsFrame) {
    insertion.mMultiple = true;
  }

  return insertion;
}

// Capture state for the frame tree rooted at the frame associated with the
// content object, aContent
void
nsCSSFrameConstructor::CaptureStateForFramesOf(nsIContent* aContent,
                                               nsILayoutHistoryState* aHistoryState)
{
  if (!aHistoryState) {
    return;
  }
  nsIFrame* frame = aContent->GetPrimaryFrame();
  if (frame == mRootElementFrame) {
    frame = mRootElementFrame ?
      GetAbsoluteContainingBlock(mRootElementFrame, FIXED_POS) :
      GetRootFrame();
  }
  for ( ; frame;
        frame = nsLayoutUtils::GetNextContinuationOrIBSplitSibling(frame)) {
    CaptureFrameState(frame, aHistoryState);
  }
}

static bool
DefinitelyEqualURIsAndPrincipal(mozilla::css::URLValue* aURI1,
                                mozilla::css::URLValue* aURI2)
{
  return aURI1 == aURI2 ||
         (aURI1 && aURI2 && aURI1->DefinitelyEqualURIsAndPrincipal(*aURI2));
}

nsStyleContext*
nsCSSFrameConstructor::MaybeRecreateFramesForElement(Element* aElement)
{
  RefPtr<nsStyleContext> oldContext = GetUndisplayedContent(aElement);
  StyleDisplay oldDisplay = StyleDisplay::None;
  if (!oldContext) {
    oldContext = GetDisplayContentsStyleFor(aElement);
    if (!oldContext) {
      return nullptr;
    }
    oldDisplay = StyleDisplay::Contents;
  }

  // The parent has a frame, so try resolving a new context.
  RefPtr<nsStyleContext> newContext = mPresShell->StyleSet()->
    ResolveStyleFor(aElement, oldContext->GetParent(),
                    ConsumeStyleBehavior::Consume,
                    LazyComputeBehavior::Assert);

  if (oldDisplay == StyleDisplay::None) {
    ChangeUndisplayedContent(aElement, newContext);
  } else {
    ChangeDisplayContents(aElement, newContext);
  }

  const nsStyleDisplay* disp = newContext->StyleDisplay();
  if (oldDisplay == disp->mDisplay) {
    // We can skip trying to recreate frames here, but only if our style
    // context does not have a binding URI that differs from our old one.
    // Otherwise, we should try to recreate, because we may want to apply the
    // new binding
    if (!disp->mBinding) {
      return newContext;
    }
    const nsStyleDisplay* oldDisp = oldContext->PeekStyleDisplay();
    if (oldDisp &&
        DefinitelyEqualURIsAndPrincipal(disp->mBinding, oldDisp->mBinding)) {
      return newContext;
    }
  }

  RecreateFramesForContent(aElement, false, REMOVE_FOR_RECONSTRUCTION, nullptr);
  return nullptr;
}

static bool
IsWhitespaceFrame(nsIFrame* aFrame)
{
  MOZ_ASSERT(aFrame, "invalid argument");
  return aFrame->GetType() == nsGkAtoms::textFrame &&
    aFrame->GetContent()->TextIsOnlyWhitespace();
}

static nsIFrame*
FindFirstNonWhitespaceChild(nsIFrame* aParentFrame)
{
  nsIFrame* f = aParentFrame->PrincipalChildList().FirstChild();
  while (f && IsWhitespaceFrame(f)) {
    f = f->GetNextSibling();
  }
  return f;
}

static nsIFrame*
FindNextNonWhitespaceSibling(nsIFrame* aFrame)
{
  nsIFrame* f = aFrame;
  do {
    f = f->GetNextSibling();
  } while (f && IsWhitespaceFrame(f));
  return f;
}

static nsIFrame*
FindPreviousNonWhitespaceSibling(nsIFrame* aFrame)
{
  nsIFrame* f = aFrame;
  do {
    f = f->GetPrevSibling();
  } while (f && IsWhitespaceFrame(f));
  return f;
}

bool
nsCSSFrameConstructor::MaybeRecreateContainerForFrameRemoval(nsIFrame* aFrame,
                                                             RemoveFlags aFlags,
                                                             nsresult* aResult,
                                                             nsIContent** aDestroyedFramesFor)
{
  NS_PRECONDITION(aFrame, "Must have a frame");
  NS_PRECONDITION(aFrame->GetParent(), "Frame shouldn't be root");
  NS_PRECONDITION(aResult, "Null out param?");
  NS_PRECONDITION(aFrame == aFrame->FirstContinuation(),
                  "aFrame not the result of GetPrimaryFrame()?");

  *aDestroyedFramesFor = nullptr;

  if (IsFramePartOfIBSplit(aFrame)) {
    // The removal functions can't handle removal of an {ib} split directly; we
    // need to rebuild the containing block.
#ifdef DEBUG
    if (gNoisyContentUpdates) {
      printf("nsCSSFrameConstructor::MaybeRecreateContainerForFrameRemoval: "
             "frame=");
      nsFrame::ListTag(stdout, aFrame);
      printf(" is ib-split\n");
    }
#endif

    *aResult = ReframeContainingBlock(aFrame, aFlags, aDestroyedFramesFor);
    return true;
  }

  nsContainerFrame* insertionFrame = aFrame->GetContentInsertionFrame();
  if (insertionFrame && insertionFrame->GetType() == nsGkAtoms::legendFrame &&
      aFrame->GetParent()->GetType() == nsGkAtoms::fieldSetFrame) {
    // When we remove the legend for a fieldset, we should reframe
    // the fieldset to ensure another legend is used, if there is one
    *aResult = RecreateFramesForContent(aFrame->GetParent()->GetContent(), false,
                                        aFlags, aDestroyedFramesFor);
    return true;
  }

  if (insertionFrame &&
      aFrame->GetParent()->GetType() == nsGkAtoms::detailsFrame) {
    HTMLSummaryElement* summary =
      HTMLSummaryElement::FromContent(insertionFrame->GetContent());

    if (summary && summary->IsMainSummary()) {
      // When removing a summary, we should reframe the parent details frame to
      // ensure that another summary is used or the default summary is
      // generated.
      *aResult = RecreateFramesForContent(aFrame->GetParent()->GetContent(),
                                          false, REMOVE_FOR_RECONSTRUCTION,
                                          aDestroyedFramesFor);
      return true;
    }
  }

  // Now check for possibly needing to reconstruct due to a pseudo parent
  nsIFrame* inFlowFrame =
    (aFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW) ?
      GetPlaceholderFrameFor(aFrame) : aFrame;
  MOZ_ASSERT(inFlowFrame, "How did that happen?");
  MOZ_ASSERT(inFlowFrame == inFlowFrame->FirstContinuation(),
             "placeholder for primary frame has previous continuations?");
  nsIFrame* parent = inFlowFrame->GetParent();
  // For the case of ruby pseudo parent, effectively, only pseudo rb/rt frame
  // need to be checked here, since all other types of parent will be catched
  // by "Check ruby containers" section below.
  if (IsTableOrRubyPseudo(parent)) {
    if (FindFirstNonWhitespaceChild(parent) == inFlowFrame ||
        !FindNextNonWhitespaceSibling(inFlowFrame->LastContinuation()) ||
        // If it is a whitespace, and is the only child of the parent, the
        // pseudo parent was created for the space, and should now be removed.
        (IsWhitespaceFrame(aFrame) &&
         parent->PrincipalChildList().OnlyChild()) ||
        // If we're a table-column-group, then the OnlyChild check above is
        // not going to catch cases when we're the first child.
        (inFlowFrame->GetType() == nsGkAtoms::tableColGroupFrame &&
         parent->GetChildList(nsIFrame::kColGroupList).FirstChild() == inFlowFrame) ||
        // Similar if we're a table-caption.
        (inFlowFrame->IsTableCaption() &&
         parent->GetChildList(nsIFrame::kCaptionList).FirstChild() == inFlowFrame)) {
      // We're the first or last frame in the pseudo.  Need to reframe.
      // Good enough to recreate frames for |parent|'s content
      *aResult = RecreateFramesForContent(parent->GetContent(), true, aFlags,
                                          aDestroyedFramesFor);
      return true;
    }
  }

  // Might need to reconstruct things if this frame's nextSibling is a table
  // or ruby pseudo, since removal of this frame might mean that this pseudo
  // needs to get merged with the frame's prevSibling if that's also a table
  // or ruby pseudo.
  nsIFrame* nextSibling =
    FindNextNonWhitespaceSibling(inFlowFrame->LastContinuation());
  NS_ASSERTION(!IsTableOrRubyPseudo(inFlowFrame), "Shouldn't happen here");
  // Effectively, for the ruby pseudo sibling case, only pseudo <ruby> frame
  // need to be checked here, since all other types of such frames will have
  // a ruby container parent, and be catched by "Check ruby containers" below.
  if (nextSibling && IsTableOrRubyPseudo(nextSibling)) {
    nsIFrame* prevSibling = FindPreviousNonWhitespaceSibling(inFlowFrame);
    if (prevSibling && IsTableOrRubyPseudo(prevSibling)) {
#ifdef DEBUG
      if (gNoisyContentUpdates) {
        printf("nsCSSFrameConstructor::MaybeRecreateContainerForFrameRemoval: "
               "frame=");
        nsFrame::ListTag(stdout, aFrame);
        printf(" has a table pseudo next sibling of different type and a "
               "table pseudo prevsibling\n");
      }
#endif
      // Good enough to recreate frames for aFrame's parent's content; even if
      // aFrame's parent is a pseudo, that'll be the right content node.
      *aResult = RecreateFramesForContent(parent->GetContent(), true, aFlags,
                                          aDestroyedFramesFor);
      return true;
    }
  }

  // Check ruby containers
  nsIAtom* parentType = parent->GetType();
  if (parentType == nsGkAtoms::rubyFrame ||
      RubyUtils::IsRubyContainerBox(parentType)) {
    // In ruby containers, pseudo frames may be created from
    // whitespaces or even nothing. There are two cases we actually
    // need to handle here, but hard to check exactly:
    // 1. Status of spaces beside the frame may vary, and related
    //    frames may be constructed or destroyed accordingly.
    // 2. The type of the first child of a ruby frame determines
    //    whether a pseudo ruby base container should exist.
    *aResult = RecreateFramesForContent(parent->GetContent(), true, aFlags,
                                        aDestroyedFramesFor);
    return true;
  }

  // Might need to reconstruct things if the removed frame's nextSibling is an
  // anonymous flex item.  The removed frame might've been what divided two
  // runs of inline content into two anonymous flex items, which would now
  // need to be merged.
  // NOTE: It's fine that we've advanced nextSibling past whitespace (up above);
  // we're only interested in anonymous flex items here, and those can never
  // be adjacent to whitespace, since they absorb contiguous runs of inline
  // non-replaced content (including whitespace).
  if (nextSibling && IsAnonymousFlexOrGridItem(nextSibling)) {
    AssertAnonymousFlexOrGridItemParent(nextSibling, parent);
#ifdef DEBUG
    if (gNoisyContentUpdates) {
      printf("nsCSSFrameConstructor::MaybeRecreateContainerForFrameRemoval: "
             "frame=");
      nsFrame::ListTag(stdout, aFrame);
      printf(" has an anonymous flex item as its next sibling\n");
    }
#endif // DEBUG
    // Recreate frames for the flex container (the removed frame's parent)
    *aResult = RecreateFramesForContent(parent->GetContent(), true, aFlags,
                                        aDestroyedFramesFor);
    return true;
  }

  // Might need to reconstruct things if the removed frame's nextSibling is
  // null and its parent is an anonymous flex item. (This might be the last
  // remaining child of that anonymous flex item, which can then go away.)
  if (!nextSibling && IsAnonymousFlexOrGridItem(parent)) {
    AssertAnonymousFlexOrGridItemParent(parent, parent->GetParent());
#ifdef DEBUG
    if (gNoisyContentUpdates) {
      printf("nsCSSFrameConstructor::MaybeRecreateContainerForFrameRemoval: "
             "frame=");
      nsFrame::ListTag(stdout, aFrame);
      printf(" has an anonymous flex item as its parent\n");
    }
#endif // DEBUG
    // Recreate frames for the flex container (the removed frame's grandparent)
    *aResult = RecreateFramesForContent(parent->GetParent()->GetContent(), true,
                                        aFlags, aDestroyedFramesFor);
    return true;
  }

#ifdef MOZ_XUL
  if (aFrame->GetType() == nsGkAtoms::popupSetFrame) {
    nsIRootBox* rootBox = nsIRootBox::GetRootBox(mPresShell);
    if (rootBox && rootBox->GetPopupSetFrame() == aFrame) {
      *aResult = ReconstructDocElementHierarchy();
      return true;
    }
  }
#endif

  // Reconstruct if inflowFrame is parent's only child, and parent is, or has,
  // a non-fluid continuation, i.e. it was split by bidi resolution
  if (!inFlowFrame->GetPrevSibling() &&
      !inFlowFrame->GetNextSibling() &&
      ((parent->GetPrevContinuation() && !parent->GetPrevInFlow()) ||
       (parent->GetNextContinuation() && !parent->GetNextInFlow()))) {
    *aResult = RecreateFramesForContent(parent->GetContent(), true, aFlags,
                                        aDestroyedFramesFor);
    return true;
  }

  // We might still need to reconstruct things if the parent of inFlowFrame is
  // ib-split, since in that case the removal of aFrame might affect the
  // splitting of its parent.
  if (!IsFramePartOfIBSplit(parent)) {
    return false;
  }

  // If inFlowFrame is not the only in-flow child of |parent|, then removing
  // it will change nothing about the {ib} split.
  if (inFlowFrame != parent->PrincipalChildList().FirstChild() ||
      inFlowFrame->LastContinuation()->GetNextSibling()) {
    return false;
  }

  // If the parent is the first or last part of the {ib} split, then
  // removing one of its kids will have no effect on the splitting.
  // Get the first continuation up front so we don't have to do it twice.
  nsIFrame* parentFirstContinuation = parent->FirstContinuation();
  if (!GetIBSplitSibling(parentFirstContinuation) ||
      !GetIBSplitPrevSibling(parentFirstContinuation)) {
    return false;
  }

#ifdef DEBUG
  if (gNoisyContentUpdates) {
    printf("nsCSSFrameConstructor::MaybeRecreateContainerForFrameRemoval: "
           "frame=");
    nsFrame::ListTag(stdout, parent);
    printf(" is ib-split\n");
  }
#endif

  *aResult = ReframeContainingBlock(parent, aFlags, aDestroyedFramesFor);
  return true;
}

nsresult
nsCSSFrameConstructor::RecreateFramesForContent(nsIContent*  aContent,
                                                bool         aAsyncInsert,
                                                RemoveFlags  aFlags,
                                                nsIContent** aDestroyedFramesFor)
{
  NS_PRECONDITION(!aAsyncInsert || aContent->IsElement(),
                  "Can only insert elements async");
  // If there is no document, we don't want to recreate frames for it.  (You
  // shouldn't generally be giving this method content without a document
  // anyway).
  // Rebuilding the frame tree can have bad effects, especially if it's the
  // frame tree for chrome (see bug 157322).
  NS_ENSURE_TRUE(aContent->GetComposedDoc(), NS_ERROR_FAILURE);

  // Is the frame ib-split? If so, we need to reframe the containing
  // block *here*, rather than trying to remove and re-insert the
  // content (which would otherwise result in *two* nested reframe
  // containing block from ContentRemoved() and ContentInserted(),
  // below!).  We'd really like to optimize away one of those
  // containing block reframes, hence the code here.

  nsIFrame* frame = aContent->GetPrimaryFrame();
  if (frame && frame->IsFrameOfType(nsIFrame::eMathML)) {
    // Reframe the topmost MathML element to prevent exponential blowup
    // (see bug 397518)
    while (true) {
      nsIContent* parentContent = aContent->GetParent();
      nsIFrame* parentContentFrame = parentContent->GetPrimaryFrame();
      if (!parentContentFrame || !parentContentFrame->IsFrameOfType(nsIFrame::eMathML))
        break;
      aContent = parentContent;
      frame = parentContentFrame;
    }
  }

  if (frame) {
    nsIFrame* nonGeneratedAncestor = nsLayoutUtils::GetNonGeneratedAncestor(frame);
    if (nonGeneratedAncestor->GetContent() != aContent) {
      return RecreateFramesForContent(nonGeneratedAncestor->GetContent(), aAsyncInsert,
                                      aFlags, aDestroyedFramesFor);
    }

    if (frame->GetStateBits() & NS_FRAME_ANONYMOUSCONTENTCREATOR_CONTENT) {
      // Recreate the frames for the entire nsIAnonymousContentCreator tree
      // since |frame| or one of its descendants may need an nsStyleContext
      // that associates it to a CSS pseudo-element, and only the
      // nsIAnonymousContentCreator that created this content knows how to make
      // that happen.
      nsIAnonymousContentCreator* acc = nullptr;
      nsIFrame* ancestor = nsLayoutUtils::GetParentOrPlaceholderFor(frame);
      while (!(acc = do_QueryFrame(ancestor))) {
        ancestor = nsLayoutUtils::GetParentOrPlaceholderFor(ancestor);
      }
      NS_ASSERTION(acc, "Where is the nsIAnonymousContentCreator? We may fail "
                        "to recreate its content correctly");
      // nsSVGUseFrame is special, and we know this is unnecessary for it.
      if (ancestor->GetType() != nsGkAtoms::svgUseFrame) {
        NS_ASSERTION(aContent->IsInNativeAnonymousSubtree(),
                     "Why is NS_FRAME_ANONYMOUSCONTENTCREATOR_CONTENT set?");
        return RecreateFramesForContent(ancestor->GetContent(), aAsyncInsert,
                                        aFlags, aDestroyedFramesFor);
      }
    }

    nsIFrame* parent = frame->GetParent();
    nsIContent* parentContent = parent ? parent->GetContent() : nullptr;
    // If the parent frame is a leaf then the subsequent insert will fail to
    // create a frame, so we need to recreate the parent content. This happens
    // with native anonymous content from the editor.
    if (parent && parent->IsLeaf() && parentContent &&
        parentContent != aContent) {
      return RecreateFramesForContent(parentContent, aAsyncInsert, aFlags,
                                      aDestroyedFramesFor);
    }
  }

  nsresult rv = NS_OK;
  nsIContent* container;
  if (frame && MaybeRecreateContainerForFrameRemoval(frame, aFlags, &rv,
                                                     &container)) {
    MOZ_ASSERT(container);
    if (aDestroyedFramesFor) {
      *aDestroyedFramesFor = container;
    }
    return rv;
  }

  nsINode* containerNode = aContent->GetParentNode();
  // XXXbz how can containerNode be null here?
  if (containerNode) {
    // Before removing the frames associated with the content object,
    // ask them to save their state onto a temporary state object.
    CaptureStateForFramesOf(aContent, mTempFrameTreeState);

    // Need the nsIContent parent, which might be null here, since we need to
    // pass it to ContentInserted and ContentRemoved.
    nsCOMPtr<nsIContent> container = aContent->GetParent();

    // Remove the frames associated with the content object.
    bool didReconstruct;
    nsIContent* nextSibling = aContent->IsRootOfAnonymousSubtree() ?
      nullptr : aContent->GetNextSibling();
    const bool reconstruct = aFlags != REMOVE_DESTROY_FRAMES;
    RemoveFlags flags = reconstruct ? REMOVE_FOR_RECONSTRUCTION : aFlags;
    rv = ContentRemoved(container, aContent, nextSibling, flags,
                        &didReconstruct, aDestroyedFramesFor);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (reconstruct && !didReconstruct) {
      // Now, recreate the frames associated with this content object. If
      // ContentRemoved triggered reconstruction, then we don't need to do this
      // because the frames will already have been built.
      if (aAsyncInsert) {
        // XXXmats doesn't frame state need to be restored in this case too?
        RestyleManager()->PostRestyleEvent(
          aContent->AsElement(), nsRestyleHint(0), nsChangeHint_ReconstructFrame);
      } else {
        rv = ContentInserted(container, aContent, mTempFrameTreeState, false);
      }
    }
  }

  return rv;
}

void
nsCSSFrameConstructor::DestroyFramesFor(nsIContent*  aContent,
                                        nsIContent** aDestroyedFramesFor)
{
  MOZ_ASSERT(aContent && aContent->GetParentNode());

  bool didReconstruct;
  nsIContent* nextSibling =
    aContent->IsRootOfAnonymousSubtree() ? nullptr : aContent->GetNextSibling();
  ContentRemoved(aContent->GetParent(), aContent, nextSibling,
                 REMOVE_DESTROY_FRAMES, &didReconstruct, aDestroyedFramesFor);
}

//////////////////////////////////////////////////////////////////////

// Block frame construction code

already_AddRefed<nsStyleContext>
nsCSSFrameConstructor::GetFirstLetterStyle(nsIContent* aContent,
                                           nsStyleContext* aStyleContext)
{
  if (aContent) {
    return mPresShell->StyleSet()->
      ResolvePseudoElementStyle(aContent->AsElement(),
                                CSSPseudoElementType::firstLetter,
                                aStyleContext,
                                nullptr);
  }
  return nullptr;
}

already_AddRefed<nsStyleContext>
nsCSSFrameConstructor::GetFirstLineStyle(nsIContent* aContent,
                                         nsStyleContext* aStyleContext)
{
  if (aContent) {
    return mPresShell->StyleSet()->
      ResolvePseudoElementStyle(aContent->AsElement(),
                                CSSPseudoElementType::firstLine,
                                aStyleContext,
                                nullptr);
  }
  return nullptr;
}

// Predicate to see if a given content (block element) has
// first-letter style applied to it.
bool
nsCSSFrameConstructor::ShouldHaveFirstLetterStyle(nsIContent* aContent,
                                                  nsStyleContext* aStyleContext)
{
  return nsLayoutUtils::HasPseudoStyle(aContent, aStyleContext,
                                       CSSPseudoElementType::firstLetter,
                                       mPresShell->GetPresContext());
}

bool
nsCSSFrameConstructor::HasFirstLetterStyle(nsIFrame* aBlockFrame)
{
  NS_PRECONDITION(aBlockFrame, "Need a frame");
  NS_ASSERTION(nsLayoutUtils::GetAsBlock(aBlockFrame),
               "Not a block frame?");

  return (aBlockFrame->GetStateBits() & NS_BLOCK_HAS_FIRST_LETTER_STYLE) != 0;
}

bool
nsCSSFrameConstructor::ShouldHaveFirstLineStyle(nsIContent* aContent,
                                                nsStyleContext* aStyleContext)
{
  bool hasFirstLine =
    nsLayoutUtils::HasPseudoStyle(aContent, aStyleContext,
                                  CSSPseudoElementType::firstLine,
                                  mPresShell->GetPresContext());
  if (hasFirstLine) {
    // But disable for fieldsets
    int32_t namespaceID;
    nsIAtom* tag = mDocument->BindingManager()->ResolveTag(aContent,
                                                           &namespaceID);
    // This check must match the one in FindHTMLData.
    hasFirstLine = tag != nsGkAtoms::fieldset ||
      namespaceID != kNameSpaceID_XHTML;
  }

  return hasFirstLine;
}

void
nsCSSFrameConstructor::ShouldHaveSpecialBlockStyle(nsIContent* aContent,
                                                   nsStyleContext* aStyleContext,
                                                   bool* aHaveFirstLetterStyle,
                                                   bool* aHaveFirstLineStyle)
{
  *aHaveFirstLetterStyle =
    ShouldHaveFirstLetterStyle(aContent, aStyleContext);
  *aHaveFirstLineStyle =
    ShouldHaveFirstLineStyle(aContent, aStyleContext);
}

/* static */
const nsCSSFrameConstructor::PseudoParentData
nsCSSFrameConstructor::sPseudoParentData[eParentTypeCount] = {
  { // Cell
    FULL_CTOR_FCDATA(FCDATA_IS_TABLE_PART | FCDATA_SKIP_FRAMESET |
                     FCDATA_USE_CHILD_ITEMS |
                     FCDATA_DESIRED_PARENT_TYPE_TO_BITS(eTypeRow),
                     &nsCSSFrameConstructor::ConstructTableCell),
    &nsCSSAnonBoxes::tableCell
  },
  { // Row
    FULL_CTOR_FCDATA(FCDATA_IS_TABLE_PART | FCDATA_SKIP_FRAMESET |
                     FCDATA_USE_CHILD_ITEMS |
                     FCDATA_DESIRED_PARENT_TYPE_TO_BITS(eTypeRowGroup),
                     &nsCSSFrameConstructor::ConstructTableRowOrRowGroup),
    &nsCSSAnonBoxes::tableRow
  },
  { // Row group
    FULL_CTOR_FCDATA(FCDATA_IS_TABLE_PART | FCDATA_SKIP_FRAMESET |
                     FCDATA_USE_CHILD_ITEMS |
                     FCDATA_DESIRED_PARENT_TYPE_TO_BITS(eTypeTable),
                     &nsCSSFrameConstructor::ConstructTableRowOrRowGroup),
    &nsCSSAnonBoxes::tableRowGroup
  },
  { // Column group
    FCDATA_DECL(FCDATA_IS_TABLE_PART | FCDATA_SKIP_FRAMESET |
                FCDATA_DISALLOW_OUT_OF_FLOW | FCDATA_USE_CHILD_ITEMS |
                FCDATA_SKIP_ABSPOS_PUSH |
                FCDATA_DESIRED_PARENT_TYPE_TO_BITS(eTypeTable),
                NS_NewTableColGroupFrame),
    &nsCSSAnonBoxes::tableColGroup
  },
  { // Table
    FULL_CTOR_FCDATA(FCDATA_SKIP_FRAMESET | FCDATA_USE_CHILD_ITEMS,
                     &nsCSSFrameConstructor::ConstructTable),
    &nsCSSAnonBoxes::table
  },
  { // Ruby
    FCDATA_DECL(FCDATA_IS_LINE_PARTICIPANT |
                FCDATA_USE_CHILD_ITEMS |
                FCDATA_SKIP_FRAMESET,
                NS_NewRubyFrame),
    &nsCSSAnonBoxes::ruby
  },
  { // Ruby Base
    FCDATA_DECL(FCDATA_USE_CHILD_ITEMS |
                FCDATA_IS_LINE_PARTICIPANT |
                FCDATA_DESIRED_PARENT_TYPE_TO_BITS(eTypeRubyBaseContainer) |
                FCDATA_SKIP_FRAMESET,
                NS_NewRubyBaseFrame),
    &nsCSSAnonBoxes::rubyBase
  },
  { // Ruby Base Container
    FCDATA_DECL(FCDATA_USE_CHILD_ITEMS |
                FCDATA_IS_LINE_PARTICIPANT |
                FCDATA_DESIRED_PARENT_TYPE_TO_BITS(eTypeRuby) |
                FCDATA_SKIP_FRAMESET,
                NS_NewRubyBaseContainerFrame),
    &nsCSSAnonBoxes::rubyBaseContainer
  },
  { // Ruby Text
    FCDATA_DECL(FCDATA_USE_CHILD_ITEMS |
                FCDATA_IS_LINE_PARTICIPANT |
                FCDATA_DESIRED_PARENT_TYPE_TO_BITS(eTypeRubyTextContainer) |
                FCDATA_SKIP_FRAMESET,
                NS_NewRubyTextFrame),
    &nsCSSAnonBoxes::rubyText
  },
  { // Ruby Text Container
    FCDATA_DECL(FCDATA_USE_CHILD_ITEMS |
                FCDATA_DESIRED_PARENT_TYPE_TO_BITS(eTypeRuby) |
                FCDATA_SKIP_FRAMESET,
                NS_NewRubyTextContainerFrame),
    &nsCSSAnonBoxes::rubyTextContainer
  }
};

void
nsCSSFrameConstructor::CreateNeededAnonFlexOrGridItems(
  nsFrameConstructorState& aState,
  FrameConstructionItemList& aItems,
  nsIFrame* aParentFrame)
{
  if (aItems.IsEmpty()) {
    return;
  }
  const nsIAtom* parentType = aParentFrame->GetType();
  if (parentType != nsGkAtoms::flexContainerFrame &&
      parentType != nsGkAtoms::gridContainerFrame) {
    return;
  }

  const bool isWebkitBox = IsFlexContainerForLegacyBox(aParentFrame,
                                                       parentType);
  FCItemIterator iter(aItems);
  do {
    // Advance iter past children that don't want to be wrapped
    if (iter.SkipItemsThatDontNeedAnonFlexOrGridItem(aState, isWebkitBox)) {
      // Hit the end of the items without finding any remaining children that
      // need to be wrapped. We're finished!
      return;
    }

    // If our next potentially-wrappable child is whitespace, then see if
    // there's anything wrappable immediately after it. If not, we just drop
    // the whitespace and move on. (We're not supposed to create any anonymous
    // flex/grid items that _only_ contain whitespace).
    // (BUT if this is generated content, then we don't give whitespace nodes
    // any special treatment, because they're probably not really whitespace --
    // they're just temporarily empty, waiting for their generated text.)
    // XXXdholbert If this node's generated text will *actually end up being
    // entirely whitespace*, then we technically should still skip over it, per
    // the CSS grid & flexbox specs. I'm not bothering with that at this point,
    // since it's a pretty extreme edge case.
    if (!aParentFrame->IsGeneratedContentFrame() &&
        iter.item().IsWhitespace(aState)) {
      FCItemIterator afterWhitespaceIter(iter);
      bool hitEnd = afterWhitespaceIter.SkipWhitespace(aState);
      bool nextChildNeedsAnonItem =
        !hitEnd &&
        afterWhitespaceIter.item().NeedsAnonFlexOrGridItem(aState, isWebkitBox);

      if (!nextChildNeedsAnonItem) {
        // There's nothing after the whitespace that we need to wrap, so we
        // just drop this run of whitespace.
        iter.DeleteItemsTo(afterWhitespaceIter);
        if (hitEnd) {
          // Nothing left to do -- we're finished!
          return;
        }
        // else, we have a next child and it does not want to be wrapped.  So,
        // we jump back to the beginning of the loop to skip over that child
        // (and anything else non-wrappable after it)
        MOZ_ASSERT(!iter.IsDone() &&
                   !iter.item().NeedsAnonFlexOrGridItem(aState, isWebkitBox),
                   "hitEnd and/or nextChildNeedsAnonItem lied");
        continue;
      }
    }

    // Now |iter| points to the first child that needs to be wrapped in an
    // anonymous flex/grid item. Now we see how many children after it also want
    // to be wrapped in an anonymous flex/grid item.
    FCItemIterator endIter(iter); // iterator to find the end of the group
    endIter.SkipItemsThatNeedAnonFlexOrGridItem(aState, isWebkitBox);

    NS_ASSERTION(iter != endIter,
                 "Should've had at least one wrappable child to seek past");

    // Now, we create the anonymous flex or grid item to contain the children
    // between |iter| and |endIter|.
    nsIAtom* pseudoType = (aParentFrame->GetType() == nsGkAtoms::flexContainerFrame) ?
      nsCSSAnonBoxes::anonymousFlexItem : nsCSSAnonBoxes::anonymousGridItem;
    nsStyleContext* parentStyle = aParentFrame->StyleContext();
    nsIContent* parentContent = aParentFrame->GetContent();
    already_AddRefed<nsStyleContext> wrapperStyle =
      mPresShell->StyleSet()->ResolveAnonymousBoxStyle(pseudoType, parentStyle);

    static const FrameConstructionData sBlockFormattingContextFCData =
      FCDATA_DECL(FCDATA_SKIP_FRAMESET | FCDATA_USE_CHILD_ITEMS,
                  NS_NewBlockFormattingContext);

    FrameConstructionItem* newItem =
      new FrameConstructionItem(&sBlockFormattingContextFCData,
                                // Use the content of our parent frame
                                parentContent,
                                // Lie about the tag; it doesn't matter anyway
                                pseudoType,
                                iter.item().mNameSpaceID,
                                // no pending binding
                                nullptr,
                                wrapperStyle,
                                true, nullptr);

    newItem->mIsAllInline = newItem->mHasInlineEnds =
      newItem->mStyleContext->StyleDisplay()->IsInlineOutsideStyle();
    newItem->mIsBlock = !newItem->mIsAllInline;

    MOZ_ASSERT(!newItem->mIsAllInline && newItem->mIsBlock,
               "expecting anonymous flex/grid items to be block-level "
               "(this will make a difference when we encounter "
               "'align-items: baseline')");

    // Anonymous flex and grid items induce line boundaries around their
    // contents.
    newItem->mChildItems.SetLineBoundaryAtStart(true);
    newItem->mChildItems.SetLineBoundaryAtEnd(true);
    // The parent of the items in aItems is also the parent of the items
    // in mChildItems
    newItem->mChildItems.SetParentHasNoXBLChildren(
      aItems.ParentHasNoXBLChildren());

    // Eat up all items between |iter| and |endIter| and put them in our
    // wrapper. This advances |iter| to point to |endIter|.
    iter.AppendItemsToList(endIter, newItem->mChildItems);

    iter.InsertItem(newItem);
  } while (!iter.IsDone());
}

/* static */ nsCSSFrameConstructor::RubyWhitespaceType
nsCSSFrameConstructor::ComputeRubyWhitespaceType(StyleDisplay aPrevDisplay,
                                                 StyleDisplay aNextDisplay)
{
  MOZ_ASSERT(nsStyleDisplay::IsRubyDisplayType(aPrevDisplay) &&
             nsStyleDisplay::IsRubyDisplayType(aNextDisplay));
  if (aPrevDisplay == aNextDisplay &&
      (aPrevDisplay == StyleDisplay::RubyBase ||
       aPrevDisplay == StyleDisplay::RubyText)) {
    return eRubyInterLeafWhitespace;
  }
  if (aNextDisplay == StyleDisplay::RubyText ||
      aNextDisplay == StyleDisplay::RubyTextContainer) {
    return eRubyInterLevelWhitespace;
  }
  return eRubyInterSegmentWhitespace;
}

/**
 * This function checks the content from |aStartIter| to |aEndIter|,
 * determines whether it contains only whitespace, and if yes,
 * interprets the type of whitespace. This method does not change
 * any of the iters.
 */
/* static */ nsCSSFrameConstructor::RubyWhitespaceType
nsCSSFrameConstructor::InterpretRubyWhitespace(nsFrameConstructorState& aState,
                                               const FCItemIterator& aStartIter,
                                               const FCItemIterator& aEndIter)
{
  if (!aStartIter.item().IsWhitespace(aState)) {
    return eRubyNotWhitespace;
  }

  FCItemIterator spaceEndIter(aStartIter);
  spaceEndIter.SkipWhitespace(aState);
  if (spaceEndIter != aEndIter) {
    return eRubyNotWhitespace;
  }

  // Any leading or trailing whitespace in non-pseudo ruby box
  // should have been trimmed, hence there should not be any
  // whitespace at the start or the end.
  MOZ_ASSERT(!aStartIter.AtStart() && !aEndIter.IsDone());
  FCItemIterator prevIter(aStartIter);
  prevIter.Prev();
  return ComputeRubyWhitespaceType(
      prevIter.item().mStyleContext->StyleDisplay()->mDisplay,
      aEndIter.item().mStyleContext->StyleDisplay()->mDisplay);
}


/**
 * This function eats up consecutive items which do not want the current
 * parent into either a ruby base box or a ruby text box.  When it
 * returns, |aIter| points to the first item it doesn't wrap.
 */
void
nsCSSFrameConstructor::WrapItemsInPseudoRubyLeafBox(
  FCItemIterator& aIter,
  nsStyleContext* aParentStyle, nsIContent* aParentContent)
{
  StyleDisplay parentDisplay = aParentStyle->StyleDisplay()->mDisplay;
  ParentType parentType, wrapperType;
  if (parentDisplay == StyleDisplay::RubyTextContainer) {
    parentType = eTypeRubyTextContainer;
    wrapperType = eTypeRubyText;
  } else {
    MOZ_ASSERT(parentDisplay == StyleDisplay::RubyBaseContainer);
    parentType = eTypeRubyBaseContainer;
    wrapperType = eTypeRubyBase;
  }

  MOZ_ASSERT(aIter.item().DesiredParentType() != parentType,
             "Should point to something needs to be wrapped.");

  FCItemIterator endIter(aIter);
  endIter.SkipItemsNotWantingParentType(parentType);

  WrapItemsInPseudoParent(aParentContent, aParentStyle,
                          wrapperType, aIter, endIter);
}

/**
 * This function eats up consecutive items into a ruby level container.
 * It may create zero or one level container. When it returns, |aIter|
 * points to the first item it doesn't wrap.
 */
void
nsCSSFrameConstructor::WrapItemsInPseudoRubyLevelContainer(
  nsFrameConstructorState& aState, FCItemIterator& aIter,
  nsStyleContext* aParentStyle, nsIContent* aParentContent)
{
  MOZ_ASSERT(aIter.item().DesiredParentType() != eTypeRuby,
             "Pointing to a level container?");

  FrameConstructionItem& firstItem = aIter.item();
  ParentType wrapperType = firstItem.DesiredParentType();
  if (wrapperType != eTypeRubyTextContainer) {
    // If the first item is not ruby text,
    // it should be in a base container.
    wrapperType = eTypeRubyBaseContainer;
  }

  FCItemIterator endIter(aIter);
  do {
    if (endIter.SkipItemsWantingParentType(wrapperType) ||
        // If the skipping above stops at some item which wants a
        // different ruby parent, then we have finished.
        IsRubyParentType(endIter.item().DesiredParentType())) {
      // No more items need to be wrapped in this level container.
      break;
    }

    FCItemIterator contentEndIter(endIter);
    contentEndIter.SkipItemsNotWantingRubyParent();
    // endIter must be on something doesn't want a ruby parent.
    MOZ_ASSERT(contentEndIter != endIter);

    // InterpretRubyWhitespace depends on the fact that any leading or
    // trailing whitespace described in the spec have been trimmed at
    // this point. With this precondition, it is safe not to check
    // whether contentEndIter has been done.
    RubyWhitespaceType whitespaceType =
      InterpretRubyWhitespace(aState, endIter, contentEndIter);
    if (whitespaceType == eRubyInterLevelWhitespace) {
      // Remove inter-level whitespace.
      bool atStart = (aIter == endIter);
      endIter.DeleteItemsTo(contentEndIter);
      if (atStart) {
        aIter = endIter;
      }
    } else if (whitespaceType == eRubyInterSegmentWhitespace) {
      // If this level container starts with inter-segment whitespaces,
      // wrap them. Break at contentEndIter. Otherwise, leave it here.
      // Break at endIter. They will be wrapped when we are here again.
      if (aIter == endIter) {
        MOZ_ASSERT(wrapperType == eTypeRubyBaseContainer,
                   "Inter-segment whitespace should be wrapped in rbc");
        endIter = contentEndIter;
      }
      break;
    } else if (wrapperType == eTypeRubyTextContainer &&
               whitespaceType != eRubyInterLeafWhitespace) {
      // Misparented inline content that's not inter-annotation
      // whitespace doesn't belong in a pseudo ruby text container.
      // Break at endIter.
      break;
    } else {
      endIter = contentEndIter;
    }
  } while (!endIter.IsDone());

  // It is possible that everything our parent wants us to wrap is
  // simply an inter-level whitespace, which has been trimmed, or
  // an inter-segment whitespace, which will be wrapped later.
  // In those cases, don't create anything.
  if (aIter != endIter) {
    WrapItemsInPseudoParent(aParentContent, aParentStyle,
                            wrapperType, aIter, endIter);
  }
}

/**
 * This function trims leading and trailing whitespaces
 * in the given item list.
 */
void
nsCSSFrameConstructor::TrimLeadingAndTrailingWhitespaces(
    nsFrameConstructorState& aState,
    FrameConstructionItemList& aItems)
{
  FCItemIterator iter(aItems);
  if (!iter.IsDone() &&
      iter.item().IsWhitespace(aState)) {
    FCItemIterator spaceEndIter(iter);
    spaceEndIter.SkipWhitespace(aState);
    iter.DeleteItemsTo(spaceEndIter);
  }

  iter.SetToEnd();
  if (!iter.AtStart()) {
    FCItemIterator spaceEndIter(iter);
    do {
      iter.Prev();
      if (iter.AtStart()) {
        // It's fine to not check the first item, because we
        // should have trimmed leading whitespaces above.
        break;
      }
    } while (iter.item().IsWhitespace(aState));
    iter.Next();
    if (iter != spaceEndIter) {
      iter.DeleteItemsTo(spaceEndIter);
    }
  }
}

/**
 * This function walks through the child list (aItems) and creates
 * needed pseudo ruby boxes to wrap misparented children.
 */
void
nsCSSFrameConstructor::CreateNeededPseudoInternalRubyBoxes(
  nsFrameConstructorState& aState,
  FrameConstructionItemList& aItems,
  nsIFrame* aParentFrame)
{
  const ParentType ourParentType = GetParentType(aParentFrame);
  if (!IsRubyParentType(ourParentType) ||
      aItems.AllWantParentType(ourParentType)) {
    return;
  }

  if (!IsRubyPseudo(aParentFrame)) {
    // Normally, ruby pseudo frames start from and end at some elements,
    // which means they don't have leading and trailing whitespaces at
    // all.  But there are two cases where they do actually have leading
    // or trailing whitespaces:
    // 1. It is an inter-segment whitespace which in an individual ruby
    //    base container.
    // 2. The pseudo frame starts from or ends at consecutive inline
    //    content, which is not pure whitespace, but includes some.
    // In either case, the whitespaces are not the leading or trailing
    // whitespaces defined in the spec, and thus should not be trimmed.
    TrimLeadingAndTrailingWhitespaces(aState, aItems);
  }

  FCItemIterator iter(aItems);
  nsIContent* parentContent = aParentFrame->GetContent();
  nsStyleContext* parentStyle = aParentFrame->StyleContext();
  while (!iter.IsDone()) {
    if (!iter.SkipItemsWantingParentType(ourParentType)) {
      if (ourParentType == eTypeRuby) {
        WrapItemsInPseudoRubyLevelContainer(aState, iter, parentStyle,
                                            parentContent);
      } else {
        WrapItemsInPseudoRubyLeafBox(iter, parentStyle, parentContent);
      }
    }
  }
}

/*
 * This function works as follows: we walk through the child list (aItems) and
 * find items that cannot have aParentFrame as their parent.  We wrap
 * continuous runs of such items into a FrameConstructionItem for a frame that
 * gets them closer to their desired parents.  For example, a run of non-row
 * children of a row-group will get wrapped in a row.  When we later construct
 * the frame for this wrapper (in this case for the row), it'll be the correct
 * parent for the cells in the set of items we wrapped or we'll wrap cells
 * around everything else.  At the end of this method, aItems is guaranteed to
 * contain only items for frames that can be direct kids of aParentFrame.
 */
void
nsCSSFrameConstructor::CreateNeededPseudoContainers(
    nsFrameConstructorState& aState,
    FrameConstructionItemList& aItems,
    nsIFrame* aParentFrame)
{
  ParentType ourParentType = GetParentType(aParentFrame);
  if (IsRubyParentType(ourParentType) ||
      aItems.AllWantParentType(ourParentType)) {
    // Nothing to do here
    return;
  }

  FCItemIterator iter(aItems);
  do {
    if (iter.SkipItemsWantingParentType(ourParentType)) {
      // Nothing else to do here; we're finished
      return;
    }

    // Now we're pointing to the first child that wants a different parent
    // type.

    // Now try to figure out what kids we can group together.  We can generally
    // group everything that has a different desired parent type from us.  Two
    // exceptions to this:
    // 1) If our parent type is table, we can't group columns with anything
    //    else other than whitespace.
    // 2) Whitespace that lies between two things we can group which both want
    //    a non-block parent should be dropped, even if we can't group them
    //    with each other and even if the whitespace wants a parent of
    //    ourParentType.  Ends of the list count as things that don't want a
    //    block parent (so that for example we'll drop a whitespace-only list).

    FCItemIterator endIter(iter); /* iterator to find the end of the group */
    ParentType groupingParentType = endIter.item().DesiredParentType();
    if (aItems.AllWantParentType(groupingParentType) &&
        groupingParentType != eTypeBlock) {
      // Just group them all and be done with it.  We need the check for
      // eTypeBlock here to catch the "all the items are whitespace" case
      // described above.
      endIter.SetToEnd();
    } else {
      // Locate the end of the group.

      // Keep track of the type the previous item wanted, in case we have to
      // deal with whitespace.  Start it off with ourParentType, since that's
      // the last thing |iter| would have skipped over.
      ParentType prevParentType = ourParentType;
      do {
        // Walk an iterator past any whitespace that we might be able to drop
        // from the list
        FCItemIterator spaceEndIter(endIter);
        if (prevParentType != eTypeBlock &&
            !aParentFrame->IsGeneratedContentFrame() &&
            spaceEndIter.item().IsWhitespace(aState)) {
          bool trailingSpaces = spaceEndIter.SkipWhitespace(aState);

          // We drop the whitespace in the following cases:
          // 1) If these are not trailing spaces and the next item wants a table
          //    or table-part parent
          // 2) If these are trailing spaces and aParentFrame is a
          //    tabular container according to rule 1.3 of CSS 2.1 Sec 17.2.1.
          //    (Being a tabular container pretty much means ourParentType is
          //    not eTypeBlock besides the eTypeColGroup case, which won't
          //    reach here.)
          if ((!trailingSpaces &&
               IsTableParentType(spaceEndIter.item().DesiredParentType())) ||
              (trailingSpaces && ourParentType != eTypeBlock)) {
            bool updateStart = (iter == endIter);
            endIter.DeleteItemsTo(spaceEndIter);
            NS_ASSERTION(trailingSpaces == endIter.IsDone(),
                         "These should match");

            if (updateStart) {
              iter = endIter;
            }

            if (trailingSpaces) {
              break; /* Found group end */
            }

            if (updateStart) {
              // Update groupingParentType, since it might have been eTypeBlock
              // just because of the whitespace.
              groupingParentType = iter.item().DesiredParentType();
            }
          }
        }

        // Now endIter points to a non-whitespace item or a non-droppable
        // whitespace item. In the latter case, if this is the end of the group
        // we'll traverse this whitespace again.  But it'll all just be quick
        // DesiredParentType() checks which will match ourParentType (that's
        // what it means that this is the group end), so it's OK.
        // However, when we are grouping a ruby parent, and endIter points to
        // a non-droppable whitespace, if the next non-whitespace item also
        // wants a ruby parent, the whitespace should also be included into
        // the current ruby container.
        prevParentType = endIter.item().DesiredParentType();
        if (prevParentType == ourParentType &&
            (endIter == spaceEndIter ||
             spaceEndIter.IsDone() ||
             !IsRubyParentType(groupingParentType) ||
             !IsRubyParentType(spaceEndIter.item().DesiredParentType()))) {
          // End the group at endIter.
          break;
        }

        if (ourParentType == eTypeTable &&
            (prevParentType == eTypeColGroup) !=
            (groupingParentType == eTypeColGroup)) {
          // Either we started with columns and now found something else, or vice
          // versa.  In any case, end the grouping.
          break;
        }

        // If we have some whitespace that we were not able to drop and there is
        // an item after the whitespace that is already properly parented, then
        // make sure to include the spaces in our group but stop the group after
        // that.
        if (spaceEndIter != endIter &&
            !spaceEndIter.IsDone() &&
            ourParentType == spaceEndIter.item().DesiredParentType()) {
            endIter = spaceEndIter;
            break;
        }

        // Include the whitespace we didn't drop (if any) in the group.
        endIter = spaceEndIter;
        prevParentType = endIter.item().DesiredParentType();

        endIter.Next();
      } while (!endIter.IsDone());
    }

    if (iter == endIter) {
      // Nothing to wrap here; just skipped some whitespace
      continue;
    }

    // Now group together all the items between iter and endIter.  The right
    // parent type to use depends on ourParentType.
    ParentType wrapperType;
    switch (ourParentType) {
      case eTypeRow:
        // The parent type for a cell is eTypeBlock, since that's what a cell
        // looks like to its kids.
        wrapperType = eTypeBlock;
        break;
      case eTypeRowGroup:
        wrapperType = eTypeRow;
        break;
      case eTypeTable:
        // Either colgroup or rowgroup, depending on what we're grouping.
        wrapperType = groupingParentType == eTypeColGroup ?
          eTypeColGroup : eTypeRowGroup;
        break;
      case eTypeColGroup:
        MOZ_CRASH("Colgroups should be suppresing non-col child items");
      default:
        NS_ASSERTION(ourParentType == eTypeBlock, "Unrecognized parent type");
        if (IsRubyParentType(groupingParentType)) {
          wrapperType = eTypeRuby;
        } else {
          NS_ASSERTION(IsTableParentType(groupingParentType),
                       "groupingParentType should be either Ruby or table");
          wrapperType = eTypeTable;
        }
    }

    nsStyleContext* parentStyle = aParentFrame->StyleContext();
    WrapItemsInPseudoParent(aParentFrame->GetContent(), parentStyle,
                            wrapperType, iter, endIter);

    // Now |iter| points to the item that was the first one we didn't wrap;
    // loop and see whether we need to skip it or wrap it in something
    // different.
  } while (!iter.IsDone());
}

/**
 * This method wraps frame construction item from |aIter| to
 * |aEndIter|. After it returns, aIter points to the first item
 * after the wrapper.
 */
void
nsCSSFrameConstructor::WrapItemsInPseudoParent(nsIContent* aParentContent,
                                               nsStyleContext* aParentStyle,
                                               ParentType aWrapperType,
                                               FCItemIterator& aIter,
                                               const FCItemIterator& aEndIter)
{
  const PseudoParentData& pseudoData = sPseudoParentData[aWrapperType];
  nsIAtom* pseudoType = *pseudoData.mPseudoType;
  StyleDisplay parentDisplay = aParentStyle->StyleDisplay()->mDisplay;

  if (pseudoType == nsCSSAnonBoxes::table &&
      (parentDisplay == StyleDisplay::Inline ||
       parentDisplay == StyleDisplay::RubyBase ||
       parentDisplay == StyleDisplay::RubyText)) {
    pseudoType = nsCSSAnonBoxes::inlineTable;
  }

  already_AddRefed<nsStyleContext> wrapperStyle =
    mPresShell->StyleSet()->ResolveAnonymousBoxStyle(pseudoType, aParentStyle);
  FrameConstructionItem* newItem =
    new FrameConstructionItem(&pseudoData.mFCData,
                              // Use the content of our parent frame
                              aParentContent,
                              // Lie about the tag; it doesn't matter anyway
                              pseudoType,
                              // The namespace does matter, however; it needs
                              // to match that of our first child item to
                              // match the old behavior
                              aIter.item().mNameSpaceID,
                              // no pending binding
                              nullptr,
                              wrapperStyle,
                              true, nullptr);

  const nsStyleDisplay* disp = newItem->mStyleContext->StyleDisplay();
  // Here we're cheating a tad... technically, table-internal items should be
  // inline if aParentFrame is inline, but they'll get wrapped in an
  // inline-table in the end, so it'll all work out.  In any case, arguably
  // we don't need to maintain this state at this point... but it's better
  // to, I guess.
  newItem->mIsAllInline = newItem->mHasInlineEnds =
    disp->IsInlineOutsideStyle();

  bool isRuby = disp->IsRubyDisplayType();
  // All types of ruby frames need a block frame to provide line layout,
  // hence they are always line participant.
  newItem->mIsLineParticipant = isRuby;

  if (!isRuby) {
    // Table pseudo frames always induce line boundaries around their
    // contents.
    newItem->mChildItems.SetLineBoundaryAtStart(true);
    newItem->mChildItems.SetLineBoundaryAtEnd(true);
  }
  // The parent of the items in aItems is also the parent of the items
  // in mChildItems
  newItem->mChildItems.SetParentHasNoXBLChildren(
      aIter.List()->ParentHasNoXBLChildren());

  // Eat up all items between |aIter| and |aEndIter| and put them in our
  // wrapper Advances |aIter| to point to |aEndIter|.
  aIter.AppendItemsToList(aEndIter, newItem->mChildItems);

  aIter.InsertItem(newItem);
}

void nsCSSFrameConstructor::CreateNeededPseudoSiblings(
    nsFrameConstructorState& aState,
    FrameConstructionItemList& aItems,
    nsIFrame* aParentFrame)
{
  if (aItems.IsEmpty() ||
      GetParentType(aParentFrame) != eTypeRuby) {
    return;
  }

  FCItemIterator iter(aItems);
  StyleDisplay firstDisplay = iter.item().mStyleContext->StyleDisplay()->mDisplay;
  if (firstDisplay == StyleDisplay::RubyBaseContainer) {
    return;
  }
  NS_ASSERTION(firstDisplay == StyleDisplay::RubyTextContainer,
               "Child of ruby frame should either a rbc or a rtc");

  const PseudoParentData& pseudoData =
    sPseudoParentData[eTypeRubyBaseContainer];
  already_AddRefed<nsStyleContext> pseudoStyle = mPresShell->StyleSet()->
    ResolveAnonymousBoxStyle(*pseudoData.mPseudoType,
                             aParentFrame->StyleContext());
  FrameConstructionItem* newItem =
    new FrameConstructionItem(&pseudoData.mFCData,
                              // Use the content of the parent frame
                              aParentFrame->GetContent(),
                              // Tag type
                              *pseudoData.mPseudoType,
                              // Use the namespace of the rtc frame
                              iter.item().mNameSpaceID,
                              // no pending binding
                              nullptr,
                              pseudoStyle,
                              true, nullptr);
  newItem->mIsAllInline = true;
  newItem->mChildItems.SetParentHasNoXBLChildren(true);
  iter.InsertItem(newItem);
}

#ifdef DEBUG
static bool
FrameWantsToBeInAnonymousItem(const nsIAtom* aParentType, const nsIFrame* aFrame)
{
  MOZ_ASSERT(aParentType == nsGkAtoms::flexContainerFrame ||
             aParentType == nsGkAtoms::gridContainerFrame);

  return aFrame->IsFrameOfType(nsIFrame::eLineParticipant);
}
#endif

static void
VerifyGridFlexContainerChildren(nsIFrame* aParentFrame,
                                const nsFrameList& aChildren)
{
#ifdef DEBUG
  auto parentType = aParentFrame->GetType();
  if (parentType != nsGkAtoms::flexContainerFrame &&
      parentType != nsGkAtoms::gridContainerFrame) {
    return;
  }

  bool prevChildWasAnonItem = false;
  for (const nsIFrame* child : aChildren) {
    MOZ_ASSERT(!FrameWantsToBeInAnonymousItem(parentType, child),
               "frame wants to be inside an anonymous item, but it isn't");
    if (IsAnonymousFlexOrGridItem(child)) {
      AssertAnonymousFlexOrGridItemParent(child, aParentFrame);
      MOZ_ASSERT(!prevChildWasAnonItem, "two anon items in a row");
      nsIFrame* firstWrappedChild = child->PrincipalChildList().FirstChild();
      MOZ_ASSERT(firstWrappedChild, "anonymous item shouldn't be empty");
      prevChildWasAnonItem = true;
    } else {
      prevChildWasAnonItem = false;
    }
  }
#endif
}

inline void
nsCSSFrameConstructor::ConstructFramesFromItemList(nsFrameConstructorState& aState,
                                                   FrameConstructionItemList& aItems,
                                                   nsContainerFrame* aParentFrame,
                                                   nsFrameItems& aFrameItems)
{
  CreateNeededPseudoContainers(aState, aItems, aParentFrame);
  CreateNeededAnonFlexOrGridItems(aState, aItems, aParentFrame);
  CreateNeededPseudoInternalRubyBoxes(aState, aItems, aParentFrame);
  CreateNeededPseudoSiblings(aState, aItems, aParentFrame);

  aItems.SetTriedConstructingFrames();
  for (FCItemIterator iter(aItems); !iter.IsDone(); iter.Next()) {
    NS_ASSERTION(iter.item().DesiredParentType() == GetParentType(aParentFrame),
                 "Needed pseudos didn't get created; expect bad things");
    ConstructFramesFromItem(aState, iter, aParentFrame, aFrameItems);
  }

  VerifyGridFlexContainerChildren(aParentFrame, aFrameItems);
  NS_ASSERTION(!aState.mHavePendingPopupgroup,
               "Should have proccessed it by now");
}

void
nsCSSFrameConstructor::AddFCItemsForAnonymousContent(
            nsFrameConstructorState& aState,
            nsContainerFrame* aFrame,
            nsTArray<nsIAnonymousContentCreator::ContentInfo>& aAnonymousItems,
            FrameConstructionItemList& aItemsToConstruct,
            uint32_t aExtraFlags)
{
  for (uint32_t i = 0; i < aAnonymousItems.Length(); ++i) {
    nsIContent* content = aAnonymousItems[i].mContent;
#ifdef DEBUG
    nsIAnonymousContentCreator* creator = do_QueryFrame(aFrame);
    NS_ASSERTION(!creator || !creator->CreateFrameFor(content),
                 "If you need to use CreateFrameFor, you need to call "
                 "CreateAnonymousFrames manually and not follow the standard "
                 "ProcessChildren() codepath for this frame");
#endif
    // Gecko-styled nodes should have no pending restyle flags.
    MOZ_ASSERT_IF(!content->IsStyledByServo(),
                  !content->IsElement() ||
                  !(content->GetFlags() & ELEMENT_ALL_RESTYLE_FLAGS));
    // Assert some things about this content
    MOZ_ASSERT(!(content->GetFlags() &
                 (NODE_DESCENDANTS_NEED_FRAMES | NODE_NEEDS_FRAME)),
               "Should not be marked as needing frames");
    MOZ_ASSERT(!content->GetPrimaryFrame(),
               "Should have no existing frame");
    MOZ_ASSERT(!content->IsNodeOfType(nsINode::eCOMMENT) &&
               !content->IsNodeOfType(nsINode::ePROCESSING_INSTRUCTION),
               "Why is someone creating garbage anonymous content");

    RefPtr<nsStyleContext> styleContext;
    TreeMatchContext::AutoParentDisplayBasedStyleFixupSkipper
      parentDisplayBasedStyleFixupSkipper(aState.mTreeMatchContext);
    if (aAnonymousItems[i].mStyleContext) {
      // If we have an explicit style context, that means that the anonymous
      // content creator had its own plan for the style, and doesn't need the
      // computed style obtained by cascading this content as a normal node.
      // This happens when a native anonymous node is used to implement a
      // pseudo-element. Allowing Servo to traverse these nodes would be wasted
      // work, so assert that we didn't do that.
      MOZ_ASSERT_IF(content->IsStyledByServo(),
                    !content->IsElement() || !content->AsElement()->HasServoData());
      styleContext = aAnonymousItems[i].mStyleContext.forget();
    } else {
      // If we don't have an explicit style context, that means we need the
      // ordinary computed values. Make sure we eagerly cascaded them when the
      // anonymous nodes were created.
      MOZ_ASSERT_IF(content->IsStyledByServo() && content->IsElement(),
                    content->AsElement()->HasServoData());
      styleContext = ResolveStyleContext(aFrame, content, &aState);
    }

    nsTArray<nsIAnonymousContentCreator::ContentInfo>* anonChildren = nullptr;
    if (!aAnonymousItems[i].mChildren.IsEmpty()) {
      anonChildren = &aAnonymousItems[i].mChildren;
    }

    uint32_t flags = ITEM_ALLOW_XBL_BASE | ITEM_ALLOW_PAGE_BREAK |
                     ITEM_IS_ANONYMOUSCONTENTCREATOR_CONTENT | aExtraFlags;

    AddFrameConstructionItemsInternal(aState, content, aFrame,
                                      content->NodeInfo()->NameAtom(),
                                      content->GetNameSpaceID(),
                                      true, styleContext, flags,
                                      anonChildren, aItemsToConstruct);
  }
}

void
nsCSSFrameConstructor::ProcessChildren(nsFrameConstructorState& aState,
                                       nsIContent*              aContent,
                                       nsStyleContext*          aStyleContext,
                                       nsContainerFrame*        aFrame,
                                       const bool               aCanHaveGeneratedContent,
                                       nsFrameItems&            aFrameItems,
                                       const bool               aAllowBlockStyles,
                                       PendingBinding*          aPendingBinding,
                                       nsIFrame*                aPossiblyLeafFrame)
{
  NS_PRECONDITION(aFrame, "Must have parent frame here");
  NS_PRECONDITION(aFrame->GetContentInsertionFrame() == aFrame,
                  "Parent frame in ProcessChildren should be its own "
                  "content insertion frame");
  const uint32_t kMaxDepth = 2 * MAX_REFLOW_DEPTH;
  static_assert(kMaxDepth <= UINT16_MAX, "mCurrentDepth type is too narrow");
  AutoRestore<uint16_t> savedDepth(mCurrentDepth);
  if (mCurrentDepth != UINT16_MAX) {
    ++mCurrentDepth;
  }

  if (!aPossiblyLeafFrame) {
    aPossiblyLeafFrame = aFrame;
  }

  // XXXbz ideally, this would do all the pushing of various
  // containing blocks as needed, so callers don't have to do it...

  bool haveFirstLetterStyle = false, haveFirstLineStyle = false;
  if (aAllowBlockStyles) {
    ShouldHaveSpecialBlockStyle(aContent, aStyleContext, &haveFirstLetterStyle,
                                &haveFirstLineStyle);
  }

  // The logic here needs to match the logic in GetFloatContainingBlock()
  nsFrameConstructorSaveState floatSaveState;
  if (ShouldSuppressFloatingOfDescendants(aFrame)) {
    aState.PushFloatContainingBlock(nullptr, floatSaveState);
  } else if (aFrame->IsFloatContainingBlock()) {
    aState.PushFloatContainingBlock(aFrame, floatSaveState);
  }

  nsFrameConstructorState::PendingBindingAutoPusher pusher(aState,
                                                           aPendingBinding);

  FrameConstructionItemList itemsToConstruct;

  // If we have first-letter or first-line style then frames can get
  // moved around so don't set these flags.
  if (aAllowBlockStyles && !haveFirstLetterStyle && !haveFirstLineStyle) {
    itemsToConstruct.SetLineBoundaryAtStart(true);
    itemsToConstruct.SetLineBoundaryAtEnd(true);
  }

  // Create any anonymous frames we need here.  This must happen before the
  // non-anonymous children are processed to ensure that popups are never
  // constructed before the popupset.
  AutoTArray<nsIAnonymousContentCreator::ContentInfo, 4> anonymousItems;
  GetAnonymousContent(aContent, aPossiblyLeafFrame, anonymousItems);
#ifdef DEBUG
  for (uint32_t i = 0; i < anonymousItems.Length(); ++i) {
    MOZ_ASSERT(anonymousItems[i].mContent->IsRootOfAnonymousSubtree(),
               "Content should know it's an anonymous subtree");
  }
#endif
  AddFCItemsForAnonymousContent(aState, aFrame, anonymousItems,
                                itemsToConstruct);

  if (!aPossiblyLeafFrame->IsLeaf()) {
    // :before/:after content should have the same style context parent
    // as normal kids.
    // Note that we don't use this style context for looking up things like
    // special block styles because in some cases involving table pseudo-frames
    // it has nothing to do with the parent frame's desired behavior.
    nsStyleContext* styleContext;

    if (aCanHaveGeneratedContent) {
      aFrame->AddStateBits(NS_FRAME_MAY_HAVE_GENERATED_CONTENT);
      styleContext =
        nsFrame::CorrectStyleParentFrame(aFrame, nullptr)->StyleContext();
      // Probe for generated content before
      CreateGeneratedContentItem(aState, aFrame, aContent, styleContext,
                                 CSSPseudoElementType::before,
                                 itemsToConstruct);
    }

    const bool addChildItems = MOZ_LIKELY(mCurrentDepth < kMaxDepth);
    if (!addChildItems) {
      NS_WARNING("ProcessChildren max depth exceeded");
    }

    InsertionPoint insertion(aFrame, nullptr);
    FlattenedChildIterator iter(aContent);
    for (nsIContent* child = iter.GetNextChild(); child; child = iter.GetNextChild()) {
      // Get the parent of the content and check if it is a XBL children element
      // (if the content is a children element then parent != aContent because the
      // FlattenedChildIterator will transitively iterate through <xbl:children>
      // for default content). Push the children element as an ancestor here because
      // it does not have a frame and would not otherwise be pushed as an ancestor.
      insertion.mContainer = aContent;
      nsIContent* parent = child->GetParent();
      MOZ_ASSERT(parent, "Parent must be non-null because we are iterating children.");
      TreeMatchContext::AutoAncestorPusher ancestorPusher(aState.mTreeMatchContext);
      if (parent != aContent && parent->IsElement()) {
        insertion.mContainer = child->GetFlattenedTreeParent();
        MOZ_ASSERT(insertion.mContainer == GetInsertionPoint(parent, child).mContainer);
        if (aState.mTreeMatchContext.mAncestorFilter.HasFilter()) {
          ancestorPusher.PushAncestorAndStyleScope(parent->AsElement());
        } else {
          ancestorPusher.PushStyleScope(parent->AsElement());
        }
      }

      // Frame construction item construction should not post
      // restyles, so removing restyle flags here is safe.
      child->UnsetRestyleFlagsIfGecko();
      if (addChildItems) {
        AddFrameConstructionItems(aState, child, iter.XBLInvolved(), insertion,
                                  itemsToConstruct);
      } else {
        ClearLazyBits(child, child->GetNextSibling());
      }
    }
    itemsToConstruct.SetParentHasNoXBLChildren(!iter.XBLInvolved());

    if (aCanHaveGeneratedContent) {
      // Probe for generated content after
      CreateGeneratedContentItem(aState, aFrame, aContent, styleContext,
                                 CSSPseudoElementType::after,
                                 itemsToConstruct);
    }
  } else {
    ClearLazyBits(aContent->GetFirstChild(), nullptr);
  }

  ConstructFramesFromItemList(aState, itemsToConstruct, aFrame, aFrameItems);

  NS_ASSERTION(!aAllowBlockStyles || !aFrame->IsXULBoxFrame(),
               "can't be both block and box");

  if (haveFirstLetterStyle) {
    WrapFramesInFirstLetterFrame(aFrame, aFrameItems);
  }
  if (haveFirstLineStyle) {
    WrapFramesInFirstLineFrame(aState, aContent, aFrame, nullptr,
                               aFrameItems);
  }

  // We might end up with first-line frames that change
  // AnyKidsNeedBlockParent() without changing itemsToConstruct, but that
  // should never happen for cases whan aFrame->IsXULBoxFrame().
  NS_ASSERTION(!haveFirstLineStyle || !aFrame->IsXULBoxFrame(),
               "Shouldn't have first-line style if we're a box");
  NS_ASSERTION(!aFrame->IsXULBoxFrame() ||
               itemsToConstruct.AnyItemsNeedBlockParent() ==
                 (AnyKidsNeedBlockParent(aFrameItems.FirstChild()) != nullptr),
               "Something went awry in our block parent calculations");

  if (aFrame->IsXULBoxFrame() && itemsToConstruct.AnyItemsNeedBlockParent()) {
    // XXXbz we could do this on the FrameConstructionItemList level,
    // no?  And if we cared we could look through the item list
    // instead of groveling through the framelist here..
    nsStyleContext *frameStyleContext = aFrame->StyleContext();
    // Report a warning for non-GC frames, for chrome:
    if (!aFrame->IsGeneratedContentFrame() &&
        mPresShell->GetPresContext()->IsChrome()) {
      nsIContent *badKid = AnyKidsNeedBlockParent(aFrameItems.FirstChild());
      nsDependentAtomString parentTag(aContent->NodeInfo()->NameAtom()),
                            kidTag(badKid->NodeInfo()->NameAtom());
      const char16_t* params[] = { parentTag.get(), kidTag.get() };
      const nsStyleDisplay *display = frameStyleContext->StyleDisplay();
      const char *message =
        (display->mDisplay == StyleDisplay::InlineBox)
          ? "NeededToWrapXULInlineBox" : "NeededToWrapXUL";
      nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                      NS_LITERAL_CSTRING("Layout: FrameConstructor"),
                                      mDocument,
                                      nsContentUtils::eXUL_PROPERTIES,
                                      message,
                                      params, ArrayLength(params));
    }

    RefPtr<nsStyleContext> blockSC = mPresShell->StyleSet()->
      ResolveAnonymousBoxStyle(nsCSSAnonBoxes::mozXULAnonymousBlock,
                               frameStyleContext);
    nsBlockFrame* blockFrame = NS_NewBlockFrame(mPresShell, blockSC);
    // We might, in theory, want to set NS_BLOCK_FLOAT_MGR and
    // NS_BLOCK_MARGIN_ROOT, but I think it's a bad idea given that
    // a real block placed here wouldn't get those set on it.

    InitAndRestoreFrame(aState, aContent, aFrame, blockFrame, false);

    NS_ASSERTION(!blockFrame->HasView(), "need to do view reparenting");
    ReparentFrames(this, blockFrame, aFrameItems);

    blockFrame->SetInitialChildList(kPrincipalList, aFrameItems);
    NS_ASSERTION(aFrameItems.IsEmpty(), "How did that happen?");
    aFrameItems.Clear();
    aFrameItems.AddChild(blockFrame);

    aFrame->AddStateBits(NS_STATE_BOX_WRAPS_KIDS_IN_BLOCK);
  }
}

//----------------------------------------------------------------------

// Support for :first-line style

// Special routine to handle placing a list of frames into a block
// frame that has first-line style. The routine ensures that the first
// collection of inline frames end up in a first-line frame.
// NOTE: aState may have containing block information related to a
// different part of the frame tree than where the first line occurs.
// In particular aState may be set up for where ContentInserted or
// ContentAppended is inserting content, which may be some
// non-first-in-flow continuation of the block to which the first-line
// belongs. So this function needs to be careful about how it uses
// aState.
void
nsCSSFrameConstructor::WrapFramesInFirstLineFrame(
  nsFrameConstructorState& aState,
  nsIContent*              aBlockContent,
  nsContainerFrame*        aBlockFrame,
  nsFirstLineFrame*        aLineFrame,
  nsFrameItems&            aFrameItems)
{
  // Find the part of aFrameItems that we want to put in the first-line
  nsFrameList::FrameLinkEnumerator link(aFrameItems);
  while (!link.AtEnd() && link.NextFrame()->IsInlineOutside()) {
    link.Next();
  }

  nsFrameList firstLineChildren = aFrameItems.ExtractHead(link);

  if (firstLineChildren.IsEmpty()) {
    // Nothing is supposed to go into the first-line; nothing to do
    return;
  }

  if (!aLineFrame) {
    // Create line frame
    nsStyleContext* parentStyle =
      nsFrame::CorrectStyleParentFrame(aBlockFrame,
                                       nsCSSPseudoElements::firstLine)->
        StyleContext();
    RefPtr<nsStyleContext> firstLineStyle = GetFirstLineStyle(aBlockContent,
                                                                parentStyle);

    aLineFrame = NS_NewFirstLineFrame(mPresShell, firstLineStyle);

    // Initialize the line frame
    InitAndRestoreFrame(aState, aBlockContent, aBlockFrame, aLineFrame);

    // The lineFrame will be the block's first child; the rest of the
    // frame list (after lastInlineFrame) will be the second and
    // subsequent children; insert lineFrame into aFrameItems.
    aFrameItems.InsertFrame(nullptr, nullptr, aLineFrame);

    NS_ASSERTION(aLineFrame->StyleContext() == firstLineStyle,
                 "Bogus style context on line frame");
  }

  // Give the inline frames to the lineFrame <b>after</b> reparenting them
  ReparentFrames(this, aLineFrame, firstLineChildren);
  if (aLineFrame->PrincipalChildList().IsEmpty() &&
      (aLineFrame->GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    aLineFrame->SetInitialChildList(kPrincipalList, firstLineChildren);
  } else {
    AppendFrames(aLineFrame, kPrincipalList, firstLineChildren);
  }
}

// Special routine to handle appending a new frame to a block frame's
// child list. Takes care of placing the new frame into the right
// place when first-line style is present.
void
nsCSSFrameConstructor::AppendFirstLineFrames(
  nsFrameConstructorState& aState,
  nsIContent*              aBlockContent,
  nsContainerFrame*        aBlockFrame,
  nsFrameItems&            aFrameItems)
{
  // It's possible that aBlockFrame needs to have a first-line frame
  // created because it doesn't currently have any children.
  const nsFrameList& blockKids = aBlockFrame->PrincipalChildList();
  if (blockKids.IsEmpty()) {
    WrapFramesInFirstLineFrame(aState, aBlockContent,
                               aBlockFrame, nullptr, aFrameItems);
    return;
  }

  // Examine the last block child - if it's a first-line frame then
  // appended frames need special treatment.
  nsIFrame* lastBlockKid = blockKids.LastChild();
  if (lastBlockKid->GetType() != nsGkAtoms::lineFrame) {
    // No first-line frame at the end of the list, therefore there is
    // an intervening block between any first-line frame the frames
    // we are appending. Therefore, we don't need any special
    // treatment of the appended frames.
    return;
  }

  nsFirstLineFrame* lineFrame = static_cast<nsFirstLineFrame*>(lastBlockKid);
  WrapFramesInFirstLineFrame(aState, aBlockContent, aBlockFrame,
                             lineFrame, aFrameItems);
}

// Special routine to handle inserting a new frame into a block
// frame's child list. Takes care of placing the new frame into the
// right place when first-line style is present.
nsresult
nsCSSFrameConstructor::InsertFirstLineFrames(
  nsFrameConstructorState& aState,
  nsIContent*              aContent,
  nsIFrame*                aBlockFrame,
  nsContainerFrame**       aParentFrame,
  nsIFrame*                aPrevSibling,
  nsFrameItems&            aFrameItems)
{
  nsresult rv = NS_OK;
  // XXXbz If you make this method actually do something, check to
  // make sure that the caller is passing what you expect.  In
  // particular, which content is aContent?  And audit the rest of
  // this code too; it makes bogus assumptions and may not build.
#if 0
  nsIFrame* parentFrame = *aParentFrame;
  nsIFrame* newFrame = aFrameItems.childList;
  bool isInline = IsInlineOutside(newFrame);

  if (!aPrevSibling) {
    // Insertion will become the first frame. Two cases: we either
    // already have a first-line frame or we don't.
    nsIFrame* firstBlockKid = aBlockFrame->PrincipalChildList().FirstChild();
    if (firstBlockKid->GetType() == nsGkAtoms::lineFrame) {
      // We already have a first-line frame
      nsIFrame* lineFrame = firstBlockKid;

      if (isInline) {
        // Easy case: the new inline frame will go into the lineFrame.
        ReparentFrame(this, lineFrame, newFrame);
        InsertFrames(lineFrame, kPrincipalList, nullptr, newFrame);

        // Since the frame is going into the lineFrame, don't let it
        // go into the block too.
        aFrameItems.childList = nullptr;
        aFrameItems.lastChild = nullptr;
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

        if (NS_SUCCEEDED(rv)) {
          // Lookup first-line style context
          nsStyleContext* parentStyle =
            nsFrame::CorrectStyleParentFrame(aBlockFrame,
                                             nsCSSPseudoElements::firstLine)->
              StyleContext();
          RefPtr<nsStyleContext> firstLineStyle =
            GetFirstLineStyle(aContent, parentStyle);

          // Initialize the line frame
          InitAndRestoreFrame(aState, aContent, aBlockFrame, lineFrame);

          // Make sure the caller inserts the lineFrame into the
          // blocks list of children.
          aFrameItems.childList = lineFrame;
          aFrameItems.lastChild = lineFrame;

          // Give the inline frames to the lineFrame <b>after</b>
          // reparenting them
          NS_ASSERTION(lineFrame->StyleContext() == firstLineStyle,
                       "Bogus style context on line frame");
          ReparentFrame(aPresContext, lineFrame, newFrame);
          lineFrame->SetInitialChildList(kPrincipalList, newFrame);
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
          if (nextSibling) {
            nsLineFrame* lineFrame = (nsLineFrame*) prevSiblingParent;
            nsFrameList tail = lineFrame->StealFramesAfter(aPrevSibling);
            // XXX do something with 'tail'
          }

          nsLineFrame* nextLineFrame = (nsLineFrame*) lineFrame;
          for (;;) {
            nextLineFrame = nextLineFrame->GetNextInFlow();
            if (!nextLineFrame) {
              break;
            }
            nsIFrame* kids = nextLineFrame->PrincipalChildList().FirstChild();
          }
        }
        else {
          // We got lucky: aPrevSibling was the last inline frame in
          // the line-frame.
          ReparentFrame(this, aBlockFrame, newFrame);
          InsertFrames(aBlockFrame, kPrincipalList,
                       prevSiblingParent, newFrame);
          aFrameItems.childList = nullptr;
          aFrameItems.lastChild = nullptr;
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
static int32_t
FirstLetterCount(const nsTextFragment* aFragment)
{
  int32_t count = 0;
  int32_t firstLetterLength = 0;

  int32_t i, n = aFragment->GetLength();
  for (i = 0; i < n; i++) {
    char16_t ch = aFragment->CharAt(i);
    // FIXME: take content language into account when deciding whitespace.
    if (dom::IsSpaceCharacter(ch)) {
      if (firstLetterLength) {
        break;
      }
      count++;
      continue;
    }
    // XXX I18n
    if ((ch == '\'') || (ch == '\"')) {
      if (firstLetterLength) {
        break;
      }
      // keep looping
      firstLetterLength = 1;
    }
    else {
      count++;
      break;
    }
  }

  return count;
}

static bool
NeedFirstLetterContinuation(nsIContent* aContent)
{
  NS_PRECONDITION(aContent, "null ptr");

  bool result = false;
  if (aContent) {
    const nsTextFragment* frag = aContent->GetText();
    if (frag) {
      int32_t flc = FirstLetterCount(frag);
      int32_t tl = frag->GetLength();
      if (flc < tl) {
        result = true;
      }
    }
  }
  return result;
}

static bool IsFirstLetterContent(nsIContent* aContent)
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
  nsIContent* aTextContent,
  nsIFrame* aTextFrame,
  nsContainerFrame* aParentFrame,
  nsStyleContext* aStyleContext,
  nsFrameItems& aResult)
{
  nsFirstLetterFrame* letterFrame =
    NS_NewFirstLetterFrame(mPresShell, aStyleContext);
  // We don't want to use a text content for a non-text frame (because we want
  // its primary frame to be a text frame).  So use its parent for the
  // first-letter.
  nsIContent* letterContent = aTextContent->GetParent();
  nsContainerFrame* containingBlock = aState.GetGeometricParent(
    aStyleContext->StyleDisplay(), aParentFrame);
  InitAndRestoreFrame(aState, letterContent, containingBlock, letterFrame);

  // Init the text frame to refer to the letter frame. Make sure we
  // get a proper style context for it (the one passed in is for the
  // letter frame and will have the float property set on it; the text
  // frame shouldn't have that set).
  StyleSetHandle styleSet = mPresShell->StyleSet();
  RefPtr<nsStyleContext> textSC = styleSet->
    ResolveStyleForText(aTextContent, aStyleContext);
  aTextFrame->SetStyleContextWithoutNotification(textSC);
  InitAndRestoreFrame(aState, aTextContent, letterFrame, aTextFrame);

  // And then give the text frame to the letter frame
  SetInitialSingleChild(letterFrame, aTextFrame);

  // See if we will need to continue the text frame (does it contain
  // more than just the first-letter text or not?) If it does, then we
  // create (in advance) a continuation frame for it.
  nsIFrame* nextTextFrame = nullptr;
  if (NeedFirstLetterContinuation(aTextContent)) {
    // Create continuation
    nextTextFrame =
      CreateContinuingFrame(aState.mPresContext, aTextFrame, aParentFrame);
    // Repair the continuations style context
    nsStyleContext* parentStyleContext = aStyleContext->GetParent();
    if (parentStyleContext) {
      RefPtr<nsStyleContext> newSC = styleSet->
        ResolveStyleForText(aTextContent, parentStyleContext);
      nextTextFrame->SetStyleContext(newSC);
    }
  }

  NS_ASSERTION(aResult.IsEmpty(), "aResult should be an empty nsFrameItems!");
  // Put the new float before any of the floats in the block we're doing
  // first-letter for, that is, before any floats whose parent is
  // containingBlock.
  nsFrameList::FrameLinkEnumerator link(aState.mFloatedItems);
  while (!link.AtEnd() && link.NextFrame()->GetParent() != containingBlock) {
    link.Next();
  }

  aState.AddChild(letterFrame, aResult, letterContent, aStyleContext,
                  aParentFrame, false, true, false, true,
                  link.PrevFrame());

  if (nextTextFrame) {
    aResult.AddChild(nextTextFrame);
  }
}

/**
 * Create a new letter frame for aTextFrame. The letter frame will be
 * a child of aParentFrame.
 */
void
nsCSSFrameConstructor::CreateLetterFrame(nsContainerFrame* aBlockFrame,
                                         nsContainerFrame* aBlockContinuation,
                                         nsIContent* aTextContent,
                                         nsContainerFrame* aParentFrame,
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
      StyleContext();

  // Use content from containing block so that we can actually
  // find a matching style rule.
  nsIContent* blockContent = aBlockFrame->GetContent();

  // Create first-letter style rule
  RefPtr<nsStyleContext> sc = GetFirstLetterStyle(blockContent,
                                                    parentStyleContext);
  if (sc) {
    RefPtr<nsStyleContext> textSC = mPresShell->StyleSet()->
      ResolveStyleForText(aTextContent, sc);

    // Create a new text frame (the original one will be discarded)
    // pass a temporary stylecontext, the correct one will be set
    // later.  Start off by unsetting the primary frame for
    // aTextContent, so it's no longer pointing to the to-be-destroyed
    // frame.
    // XXXbz it would be really nice to destroy the old frame _first_,
    // then create the new one, so we could avoid this hack.
    aTextContent->SetPrimaryFrame(nullptr);
    nsIFrame* textFrame = NS_NewTextFrame(mPresShell, textSC);

    NS_ASSERTION(aBlockContinuation == GetFloatContainingBlock(aParentFrame),
                 "Containing block is confused");
    nsFrameConstructorState state(mPresShell,
                                  GetAbsoluteContainingBlock(aParentFrame, FIXED_POS),
                                  GetAbsoluteContainingBlock(aParentFrame, ABS_POS),
                                  aBlockContinuation);

    // Create the right type of first-letter frame
    const nsStyleDisplay* display = sc->StyleDisplay();
    if (display->IsFloatingStyle() && !aParentFrame->IsSVGText()) {
      // Make a floating first-letter frame
      CreateFloatingLetterFrame(state, aTextContent, textFrame,
                                aParentFrame, sc, aResult);
    }
    else {
      // Make an inflow first-letter frame
      nsFirstLetterFrame* letterFrame = NS_NewFirstLetterFrame(mPresShell, sc);

      // Initialize the first-letter-frame.  We don't want to use a text
      // content for a non-text frame (because we want its primary frame to
      // be a text frame).  So use its parent for the first-letter.
      nsIContent* letterContent = aTextContent->GetParent();
      letterFrame->Init(letterContent, aParentFrame, nullptr);

      InitAndRestoreFrame(state, aTextContent, letterFrame, textFrame);

      SetInitialSingleChild(letterFrame, textFrame);
      aResult.Clear();
      aResult.AddChild(letterFrame);
      NS_ASSERTION(!aBlockFrame->GetPrevContinuation(),
                   "should have the first continuation here");
      aBlockFrame->AddStateBits(NS_BLOCK_HAS_FIRST_LETTER_CHILD);
    }
    aTextContent->SetPrimaryFrame(textFrame);
  }
}

void
nsCSSFrameConstructor::WrapFramesInFirstLetterFrame(
  nsContainerFrame*        aBlockFrame,
  nsFrameItems&            aBlockFrames)
{
  aBlockFrame->AddStateBits(NS_BLOCK_HAS_FIRST_LETTER_STYLE);

  nsContainerFrame* parentFrame = nullptr;
  nsIFrame* textFrame = nullptr;
  nsIFrame* prevFrame = nullptr;
  nsFrameItems letterFrames;
  bool stopLooking = false;
  WrapFramesInFirstLetterFrame(aBlockFrame, aBlockFrame, aBlockFrame,
                               aBlockFrames.FirstChild(),
                               &parentFrame, &textFrame, &prevFrame,
                               letterFrames, &stopLooking);
  if (parentFrame) {
    if (parentFrame == aBlockFrame) {
      // Take textFrame out of the block's frame list and substitute the
      // letter frame(s) instead.
      aBlockFrames.DestroyFrame(textFrame);
      aBlockFrames.InsertFrames(nullptr, prevFrame, letterFrames);
    }
    else {
      // Take the old textFrame out of the inline parent's child list
      RemoveFrame(kPrincipalList, textFrame);

      // Insert in the letter frame(s)
      parentFrame->InsertFrames(kPrincipalList, prevFrame, letterFrames);
    }
  }
}

void
nsCSSFrameConstructor::WrapFramesInFirstLetterFrame(
  nsContainerFrame*        aBlockFrame,
  nsContainerFrame*        aBlockContinuation,
  nsContainerFrame*        aParentFrame,
  nsIFrame*                aParentFrameList,
  nsContainerFrame**       aModifiedParent,
  nsIFrame**               aTextFrame,
  nsIFrame**               aPrevFrame,
  nsFrameItems&            aLetterFrames,
  bool*                    aStopLooking)
{
  nsIFrame* prevFrame = nullptr;
  nsIFrame* frame = aParentFrameList;

  while (frame) {
    nsIFrame* nextFrame = frame->GetNextSibling();

    nsIAtom* frameType = frame->GetType();
    if (nsGkAtoms::textFrame == frameType) {
      // Wrap up first-letter content in a letter frame
      nsIContent* textContent = frame->GetContent();
      if (IsFirstLetterContent(textContent)) {
        // Create letter frame to wrap up the text
        CreateLetterFrame(aBlockFrame, aBlockContinuation, textContent,
                          aParentFrame, aLetterFrames);

        // Provide adjustment information for parent
        *aModifiedParent = aParentFrame;
        *aTextFrame = frame;
        *aPrevFrame = prevFrame;
        *aStopLooking = true;
        return;
      }
    }
    else if (IsInlineFrame(frame) && frameType != nsGkAtoms::brFrame) {
      nsIFrame* kids = frame->PrincipalChildList().FirstChild();
      WrapFramesInFirstLetterFrame(aBlockFrame, aBlockContinuation,
                                   static_cast<nsContainerFrame*>(frame),
                                   kids, aModifiedParent, aTextFrame,
                                   aPrevFrame, aLetterFrames, aStopLooking);
      if (*aStopLooking) {
        return;
      }
    }
    else {
      // This will stop us looking to create more letter frames. For
      // example, maybe the frame-type is "letterFrame" or
      // "placeholderFrame". This keeps us from creating extra letter
      // frames, and also prevents us from creating letter frames when
      // the first real content child of a block is not text (e.g. an
      // image, hr, etc.)
      *aStopLooking = true;
      break;
    }

    prevFrame = frame;
    frame = nextFrame;
  }
}

static nsIFrame*
FindFirstLetterFrame(nsIFrame* aFrame, nsIFrame::ChildListID aListID)
{
  nsFrameList list = aFrame->GetChildList(aListID);
  for (nsFrameList::Enumerator e(list); !e.AtEnd(); e.Next()) {
    if (nsGkAtoms::letterFrame == e.get()->GetType()) {
      return e.get();
    }
  }
  return nullptr;
}

nsresult
nsCSSFrameConstructor::RemoveFloatingFirstLetterFrames(
  nsIPresShell* aPresShell,
  nsIFrame* aBlockFrame)
{
  // Look for the first letter frame on the kFloatList, then kPushedFloatsList.
  nsIFrame* floatFrame =
    ::FindFirstLetterFrame(aBlockFrame, nsIFrame::kFloatList);
  if (!floatFrame) {
    floatFrame =
      ::FindFirstLetterFrame(aBlockFrame, nsIFrame::kPushedFloatsList);
    if (!floatFrame) {
      return NS_OK;
    }
  }

  // Take the text frame away from the letter frame (so it isn't
  // destroyed when we destroy the letter frame).
  nsIFrame* textFrame = floatFrame->PrincipalChildList().FirstChild();
  if (!textFrame) {
    return NS_OK;
  }

  // Discover the placeholder frame for the letter frame
  nsPlaceholderFrame* placeholderFrame = GetPlaceholderFrameFor(floatFrame);
  if (!placeholderFrame) {
    // Somethings really wrong
    return NS_OK;
  }
  nsContainerFrame* parentFrame = placeholderFrame->GetParent();
  if (!parentFrame) {
    // Somethings really wrong
    return NS_OK;
  }

  // Create a new text frame with the right style context that maps
  // all of the content that was previously part of the letter frame
  // (and probably continued elsewhere).
  nsStyleContext* parentSC = parentFrame->StyleContext();
  nsIContent* textContent = textFrame->GetContent();
  if (!textContent) {
    return NS_OK;
  }
  RefPtr<nsStyleContext> newSC = aPresShell->StyleSet()->
    ResolveStyleForText(textContent, parentSC);
  nsIFrame* newTextFrame = NS_NewTextFrame(aPresShell, newSC);
  newTextFrame->Init(textContent, parentFrame, nullptr);

  // Destroy the old text frame's continuations (the old text frame
  // will be destroyed when its letter frame is destroyed).
  nsIFrame* frameToDelete = textFrame->LastContinuation();
  while (frameToDelete != textFrame) {
    nsIFrame* nextFrameToDelete = frameToDelete->GetPrevContinuation();
    RemoveFrame(kPrincipalList, frameToDelete);
    frameToDelete = nextFrameToDelete;
  }

  nsIFrame* prevSibling = placeholderFrame->GetPrevSibling();

  // Now that everything is set...
#ifdef NOISY_FIRST_LETTER
  printf("RemoveFloatingFirstLetterFrames: textContent=%p oldTextFrame=%p newTextFrame=%p\n",
         textContent.get(), textFrame, newTextFrame);
#endif

  // Remove placeholder frame and the float
  RemoveFrame(kPrincipalList, placeholderFrame);

  // Now that the old frames are gone, we can start pointing to our
  // new primary frame.
  textContent->SetPrimaryFrame(newTextFrame);

  // Wallpaper bug 822910.
  bool offsetsNeedFixing =
    prevSibling && prevSibling->GetType() == nsGkAtoms::textFrame;
  if (offsetsNeedFixing) {
    prevSibling->AddStateBits(TEXT_OFFSETS_NEED_FIXING);
  }

  // Insert text frame in its place
  nsFrameList textList(newTextFrame, newTextFrame);
  InsertFrames(parentFrame, kPrincipalList, prevSibling, textList);

  if (offsetsNeedFixing) {
    prevSibling->RemoveStateBits(TEXT_OFFSETS_NEED_FIXING);
  }

  return NS_OK;
}

nsresult
nsCSSFrameConstructor::RemoveFirstLetterFrames(nsIPresShell* aPresShell,
                                               nsContainerFrame* aFrame,
                                               nsContainerFrame* aBlockFrame,
                                               bool* aStopLooking)
{
  nsIFrame* prevSibling = nullptr;
  nsIFrame* kid = aFrame->PrincipalChildList().FirstChild();

  while (kid) {
    if (nsGkAtoms::letterFrame == kid->GetType()) {
      // Bingo. Found it. First steal away the text frame.
      nsIFrame* textFrame = kid->PrincipalChildList().FirstChild();
      if (!textFrame) {
        break;
      }

      // Create a new textframe
      nsStyleContext* parentSC = aFrame->StyleContext();
      if (!parentSC) {
        break;
      }
      nsIContent* textContent = textFrame->GetContent();
      if (!textContent) {
        break;
      }
      RefPtr<nsStyleContext> newSC = aPresShell->StyleSet()->
        ResolveStyleForText(textContent, parentSC);
      textFrame = NS_NewTextFrame(aPresShell, newSC);
      textFrame->Init(textContent, aFrame, nullptr);

      // Next rip out the kid and replace it with the text frame
      RemoveFrame(kPrincipalList, kid);

      // Now that the old frames are gone, we can start pointing to our
      // new primary frame.
      textContent->SetPrimaryFrame(textFrame);

      // Wallpaper bug 822910.
      bool offsetsNeedFixing =
        prevSibling && prevSibling->GetType() == nsGkAtoms::textFrame;
      if (offsetsNeedFixing) {
        prevSibling->AddStateBits(TEXT_OFFSETS_NEED_FIXING);
      }

      // Insert text frame in its place
      nsFrameList textList(textFrame, textFrame);
      InsertFrames(aFrame, kPrincipalList, prevSibling, textList);

      if (offsetsNeedFixing) {
        prevSibling->RemoveStateBits(TEXT_OFFSETS_NEED_FIXING);
      }

      *aStopLooking = true;
      NS_ASSERTION(!aBlockFrame->GetPrevContinuation(),
                   "should have the first continuation here");
      aBlockFrame->RemoveStateBits(NS_BLOCK_HAS_FIRST_LETTER_CHILD);
      break;
    }
    else if (IsInlineFrame(kid)) {
      nsContainerFrame* kidAsContainerFrame = do_QueryFrame(kid);
      if (kidAsContainerFrame) {
        // Look inside child inline frame for the letter frame.
        RemoveFirstLetterFrames(aPresShell, kidAsContainerFrame,
                                aBlockFrame, aStopLooking);
        if (*aStopLooking) {
          break;
        }
      }
    }
    prevSibling = kid;
    kid = kid->GetNextSibling();
  }

  return NS_OK;
}

nsresult
nsCSSFrameConstructor::RemoveLetterFrames(nsIPresShell* aPresShell,
                                          nsContainerFrame* aBlockFrame)
{
  aBlockFrame =
    static_cast<nsContainerFrame*>(aBlockFrame->FirstContinuation());
  nsContainerFrame* continuation = aBlockFrame;

  bool stopLooking = false;
  nsresult rv;
  do {
    rv = RemoveFloatingFirstLetterFrames(aPresShell, continuation);
    if (NS_SUCCEEDED(rv)) {
      rv = RemoveFirstLetterFrames(aPresShell,
                                   continuation, aBlockFrame, &stopLooking);
    }
    if (stopLooking) {
      break;
    }
    continuation =
      static_cast<nsContainerFrame*>(continuation->GetNextContinuation());
  }  while (continuation);
  return rv;
}

// Fixup the letter frame situation for the given block
void
nsCSSFrameConstructor::RecoverLetterFrames(nsContainerFrame* aBlockFrame)
{
  aBlockFrame =
    static_cast<nsContainerFrame*>(aBlockFrame->FirstContinuation());
  nsContainerFrame* continuation = aBlockFrame;

  nsContainerFrame* parentFrame = nullptr;
  nsIFrame* textFrame = nullptr;
  nsIFrame* prevFrame = nullptr;
  nsFrameItems letterFrames;
  bool stopLooking = false;
  do {
    // XXX shouldn't this bit be set already (bug 408493), assert instead?
    continuation->AddStateBits(NS_BLOCK_HAS_FIRST_LETTER_STYLE);
    WrapFramesInFirstLetterFrame(aBlockFrame, continuation, continuation,
                                 continuation->PrincipalChildList().FirstChild(),
                                 &parentFrame, &textFrame, &prevFrame,
                                 letterFrames, &stopLooking);
    if (stopLooking) {
      break;
    }
    continuation =
      static_cast<nsContainerFrame*>(continuation->GetNextContinuation());
  } while (continuation);

  if (parentFrame) {
    // Take the old textFrame out of the parents child list
    RemoveFrame(kPrincipalList, textFrame);

    // Insert in the letter frame(s)
    parentFrame->InsertFrames(kPrincipalList, prevFrame, letterFrames);
  }
}

//----------------------------------------------------------------------

// listbox Widget Routines

nsresult
nsCSSFrameConstructor::CreateListBoxContent(nsContainerFrame*      aParentFrame,
                                            nsIFrame*              aPrevFrame,
                                            nsIContent*            aChild,
                                            nsIFrame**             aNewFrame,
                                            bool                   aIsAppend)
{
#ifdef MOZ_XUL
  nsresult rv = NS_OK;

  // Construct a new frame
  if (nullptr != aParentFrame) {
    nsFrameItems            frameItems;
    nsFrameConstructorState state(mPresShell, GetAbsoluteContainingBlock(aParentFrame, FIXED_POS),
                                  GetAbsoluteContainingBlock(aParentFrame, ABS_POS),
                                  GetFloatContainingBlock(aParentFrame),
                                  do_AddRef(mTempFrameTreeState.get()));

    // If we ever initialize the ancestor filter on |state|, make sure
    // to push the right parent!

    RefPtr<nsStyleContext> styleContext;
    styleContext = ResolveStyleContext(aParentFrame, aChild, &state);

    // Pre-check for display "none" - only if we find that, do we create
    // any frame at all
    const nsStyleDisplay* display = styleContext->StyleDisplay();

    if (StyleDisplay::None == display->mDisplay) {
      *aNewFrame = nullptr;
      return NS_OK;
    }

    BeginUpdate();

    FrameConstructionItemList items;
    AddFrameConstructionItemsInternal(state, aChild, aParentFrame,
                                      aChild->NodeInfo()->NameAtom(),
                                      aChild->GetNameSpaceID(),
                                      true, styleContext,
                                      ITEM_ALLOW_XBL_BASE, nullptr, items);
    ConstructFramesFromItemList(state, items, aParentFrame, frameItems);

    nsIFrame* newFrame = frameItems.FirstChild();
    *aNewFrame = newFrame;

    if (newFrame) {
      // Notify the parent frame
      if (aIsAppend)
        rv = ((nsListBoxBodyFrame*)aParentFrame)->ListBoxAppendFrames(frameItems);
      else
        rv = ((nsListBoxBodyFrame*)aParentFrame)->ListBoxInsertFrames(aPrevFrame, frameItems);
    }

    EndUpdate();

#ifdef ACCESSIBILITY
    if (newFrame) {
      nsAccessibilityService* accService = nsIPresShell::AccService();
      if (accService) {
        accService->ContentRangeInserted(mPresShell, aChild->GetParent(),
                                         aChild, aChild->GetNextSibling());
      }
    }
#endif
  }

  return rv;
#else
  return NS_ERROR_FAILURE;
#endif
}

//----------------------------------------

void
nsCSSFrameConstructor::ConstructBlock(nsFrameConstructorState& aState,
                                      nsIContent*              aContent,
                                      nsContainerFrame*        aParentFrame,
                                      nsContainerFrame*        aContentParentFrame,
                                      nsStyleContext*          aStyleContext,
                                      nsContainerFrame**       aNewFrame,
                                      nsFrameItems&            aFrameItems,
                                      nsIFrame*                aPositionedFrameForAbsPosContainer,
                                      PendingBinding*          aPendingBinding)
{
  // Create column wrapper if necessary
  nsContainerFrame* blockFrame = *aNewFrame;
  NS_ASSERTION((blockFrame->GetType() == nsGkAtoms::blockFrame ||
                blockFrame->GetType() == nsGkAtoms::detailsFrame),
               "not a block frame nor a details frame?");
  nsContainerFrame* parent = aParentFrame;
  RefPtr<nsStyleContext> blockStyle = aStyleContext;
  const nsStyleColumn* columns = aStyleContext->StyleColumn();

  if (columns->mColumnCount != NS_STYLE_COLUMN_COUNT_AUTO
      || columns->mColumnWidth.GetUnit() != eStyleUnit_Auto) {
    nsContainerFrame* columnSetFrame =
      NS_NewColumnSetFrame(mPresShell, aStyleContext, nsFrameState(0));

    InitAndRestoreFrame(aState, aContent, aParentFrame, columnSetFrame);
    blockStyle = mPresShell->StyleSet()->
      ResolveAnonymousBoxStyle(nsCSSAnonBoxes::columnContent, aStyleContext);
    parent = columnSetFrame;
    *aNewFrame = columnSetFrame;
    if (aPositionedFrameForAbsPosContainer == blockFrame) {
      aPositionedFrameForAbsPosContainer = columnSetFrame;
    }

    SetInitialSingleChild(columnSetFrame, blockFrame);
  }

  blockFrame->SetStyleContextWithoutNotification(blockStyle);
  InitAndRestoreFrame(aState, aContent, parent, blockFrame);

  aState.AddChild(*aNewFrame, aFrameItems, aContent, aStyleContext,
                  aContentParentFrame ? aContentParentFrame :
                                        aParentFrame);
  if (!mRootElementFrame) {
    // The frame we're constructing will be the root element frame.
    // Set mRootElementFrame before processing children.
    mRootElementFrame = *aNewFrame;
  }

  // We should make the outer frame be the absolute containing block,
  // if one is required. We have to do this because absolute
  // positioning must be computed with respect to the CSS dimensions
  // of the element, which are the dimensions of the outer block. But
  // we can't really do that because only blocks can have absolute
  // children. So use the block and try to compensate with hacks
  // in nsBlockFrame::CalculateContainingBlockSizeForAbsolutes.
  nsFrameConstructorSaveState absoluteSaveState;
  (*aNewFrame)->AddStateBits(NS_FRAME_CAN_HAVE_ABSPOS_CHILDREN);
  if (aPositionedFrameForAbsPosContainer) {
    //    NS_ASSERTION(aRelPos, "should have made area frame for this");
    aState.PushAbsoluteContainingBlock(*aNewFrame, aPositionedFrameForAbsPosContainer, absoluteSaveState);
  }

  // Process the child content
  nsFrameItems childItems;
  ProcessChildren(aState, aContent, aStyleContext, blockFrame, true,
                  childItems, true, aPendingBinding);

  // Set the frame's initial child list
  blockFrame->SetInitialChildList(kPrincipalList, childItems);
}

nsIFrame*
nsCSSFrameConstructor::ConstructInline(nsFrameConstructorState& aState,
                                       FrameConstructionItem&   aItem,
                                       nsContainerFrame*        aParentFrame,
                                       const nsStyleDisplay*    aDisplay,
                                       nsFrameItems&            aFrameItems)
{
  // If an inline frame has non-inline kids, then we chop up the child list
  // into runs of blocks and runs of inlines, create anonymous block frames to
  // contain the runs of blocks, inline frames with our style context for the
  // runs of inlines, and put all these frames, in order, into aFrameItems.  We
  // return the the first one.  The whole setup is called an {ib}
  // split; in what follows "frames in the split" refers to the anonymous blocks
  // and inlines that contain our children.
  //
  // {ib} splits maintain the following invariants:
  // 1) All frames in the split have the NS_FRAME_PART_OF_IBSPLIT bit
  //    set.
  // 2) Each frame in the split has the nsIFrame::IBSplitSibling
  //    property pointing to the next frame in the split, except for the last
  //    one, which does not have it set.
  // 3) Each frame in the split has the nsIFrame::IBSplitPrevSibling
  //    property pointing to the previous frame in the split, except for the
  //    first one, which does not have it set.
  // 4) The first and last frame in the split are always inlines.
  //
  // An invariant that is NOT maintained is that the wrappers are actually
  // linked via GetNextSibling linkage.  A simple example is an inline
  // containing an inline that contains a block.  The three parts of the inner
  // inline end up with three different parents.
  //
  // For example, this HTML:
  // <span>
  //   <div>a</div>
  //   <span>
  //     b
  //     <div>c</div>
  //   </span>
  //   d
  //   <div>e</div>
  //   f
  //  </span>
  // Gives the following frame tree:
  //
  // Inline (outer span)
  // Block (anonymous, outer span)
  //   Block (div)
  //     Text("a")
  // Inline (outer span)
  //   Inline (inner span)
  //     Text("b")
  // Block (anonymous, outer span)
  //   Block (anonymous, inner span)
  //     Block (div)
  //       Text("c")
  // Inline (outer span)
  //   Inline (inner span)
  //   Text("d")
  // Block (anonymous, outer span)
  //   Block (div)
  //     Text("e")
  // Inline (outer span)
  //   Text("f")

  nsIContent* const content = aItem.mContent;
  nsStyleContext* const styleContext = aItem.mStyleContext;

  bool positioned =
    StyleDisplay::Inline == aDisplay->mDisplay &&
    aDisplay->IsRelativelyPositionedStyle() &&
    !aParentFrame->IsSVGText();

  nsInlineFrame* newFrame = NS_NewInlineFrame(mPresShell, styleContext);

  // Initialize the frame
  InitAndRestoreFrame(aState, content, aParentFrame, newFrame);

  // Inline frames can always have generated content
  newFrame->AddStateBits(NS_FRAME_MAY_HAVE_GENERATED_CONTENT);

  nsFrameConstructorSaveState absoluteSaveState;  // definition cannot be inside next block
                                                  // because the object's destructor is significant
                                                  // this is part of the fix for bug 42372

  newFrame->AddStateBits(NS_FRAME_CAN_HAVE_ABSPOS_CHILDREN);
  if (positioned) {
    // Relatively positioned frames becomes a container for child
    // frames that are positioned
    aState.PushAbsoluteContainingBlock(newFrame, newFrame, absoluteSaveState);
  }

  // Process the child content
  nsFrameItems childItems;
  ConstructFramesFromItemList(aState, aItem.mChildItems, newFrame, childItems);

  nsFrameList::FrameLinkEnumerator firstBlockEnumerator(childItems);
  if (!aItem.mIsAllInline) {
    FindFirstBlock(firstBlockEnumerator);
  }

  if (aItem.mIsAllInline || firstBlockEnumerator.AtEnd()) {
    // This part is easy.  We either already know we have no non-inline kids,
    // or haven't found any when constructing actual frames (the latter can
    // happen only if out-of-flows that we thought had no containing block
    // acquired one when ancestor inline frames and {ib} splits got
    // constructed).  Just put all the kids into the single inline frame and
    // bail.
    newFrame->SetInitialChildList(kPrincipalList, childItems);
    aState.AddChild(newFrame, aFrameItems, content, styleContext, aParentFrame);
    return newFrame;
  }

  // This inline frame contains several types of children. Therefore this frame
  // has to be chopped into several pieces, as described above.

  // Grab the first inline's kids
  nsFrameList firstInlineKids = childItems.ExtractHead(firstBlockEnumerator);
  newFrame->SetInitialChildList(kPrincipalList, firstInlineKids);

  aFrameItems.AddChild(newFrame);

  CreateIBSiblings(aState, newFrame, positioned, childItems, aFrameItems);

  return newFrame;
}

void
nsCSSFrameConstructor::CreateIBSiblings(nsFrameConstructorState& aState,
                                        nsContainerFrame* aInitialInline,
                                        bool aIsPositioned,
                                        nsFrameItems& aChildItems,
                                        nsFrameItems& aSiblings)
{
  nsIContent* content = aInitialInline->GetContent();
  nsStyleContext* styleContext = aInitialInline->StyleContext();
  nsContainerFrame* parentFrame = aInitialInline->GetParent();

  // Resolve the right style context for our anonymous blocks.
  // The distinction in styles is needed because of CSS 2.1, section
  // 9.2.1.1, which says:
  //   When such an inline box is affected by relative positioning, any
  //   resulting translation also affects the block-level box contained
  //   in the inline box.
  RefPtr<nsStyleContext> blockSC =
    mPresShell->StyleSet()->
      ResolveAnonymousBoxStyle(aIsPositioned ?
                                 nsCSSAnonBoxes::mozAnonymousPositionedBlock :
                                 nsCSSAnonBoxes::mozAnonymousBlock,
                               styleContext);

  nsContainerFrame* lastNewInline =
    static_cast<nsContainerFrame*>(aInitialInline->FirstContinuation());
  do {
    // On entry to this loop aChildItems is not empty and the first frame in it
    // is block-level.
    NS_PRECONDITION(aChildItems.NotEmpty(), "Should have child items");
    NS_PRECONDITION(!aChildItems.FirstChild()->IsInlineOutside(),
                    "Must have list starting with block");

    // The initial run of blocks belongs to an anonymous block that we create
    // right now. The anonymous block will be the parent of these block
    // children of the inline.
    nsBlockFrame* blockFrame = NS_NewBlockFrame(mPresShell, blockSC);
    InitAndRestoreFrame(aState, content, parentFrame, blockFrame, false);

    // Find the first non-block child which defines the end of our block kids
    // and the start of our next inline's kids
    nsFrameList::FrameLinkEnumerator firstNonBlock =
      FindFirstNonBlock(aChildItems);
    nsFrameList blockKids = aChildItems.ExtractHead(firstNonBlock);

    MoveChildrenTo(aInitialInline, blockFrame, blockKids);

    SetFrameIsIBSplit(lastNewInline, blockFrame);
    aSiblings.AddChild(blockFrame);

    // Now grab the initial inlines in aChildItems and put them into an inline
    // frame.
    nsInlineFrame* inlineFrame = NS_NewInlineFrame(mPresShell, styleContext);
    InitAndRestoreFrame(aState, content, parentFrame, inlineFrame, false);
    inlineFrame->AddStateBits(NS_FRAME_MAY_HAVE_GENERATED_CONTENT |
                              NS_FRAME_CAN_HAVE_ABSPOS_CHILDREN);
    if (aIsPositioned) {
      inlineFrame->MarkAsAbsoluteContainingBlock();
    }

    if (aChildItems.NotEmpty()) {
      nsFrameList::FrameLinkEnumerator firstBlock(aChildItems);
      FindFirstBlock(firstBlock);
      nsFrameList inlineKids = aChildItems.ExtractHead(firstBlock);

      MoveChildrenTo(aInitialInline, inlineFrame, inlineKids);
    }

    SetFrameIsIBSplit(blockFrame, inlineFrame);
    aSiblings.AddChild(inlineFrame);
    lastNewInline = inlineFrame;
  } while (aChildItems.NotEmpty());

  SetFrameIsIBSplit(lastNewInline, nullptr);
}

void
nsCSSFrameConstructor::BuildInlineChildItems(nsFrameConstructorState& aState,
                                             FrameConstructionItem& aParentItem,
                                             bool aItemIsWithinSVGText,
                                             bool aItemAllowsTextPathChild)
{
  // XXXbz should we preallocate aParentItem.mChildItems to some sane
  // length?  Maybe even to parentContent->GetChildCount()?
  nsFrameConstructorState::PendingBindingAutoPusher
    pusher(aState, aParentItem.mPendingBinding);

  nsStyleContext* const parentStyleContext = aParentItem.mStyleContext;
  nsIContent* const parentContent = aParentItem.mContent;

  TreeMatchContext::AutoAncestorPusher ancestorPusher(aState.mTreeMatchContext);
  if (aState.mTreeMatchContext.mAncestorFilter.HasFilter()) {
    ancestorPusher.PushAncestorAndStyleScope(parentContent->AsElement());
  } else {
    ancestorPusher.PushStyleScope(parentContent->AsElement());
  }

  if (!aItemIsWithinSVGText) {
    // Probe for generated content before
    CreateGeneratedContentItem(aState, nullptr, parentContent, parentStyleContext,
                               CSSPseudoElementType::before,
                               aParentItem.mChildItems);
  }

  uint32_t flags = ITEM_ALLOW_XBL_BASE | ITEM_ALLOW_PAGE_BREAK;
  if (aItemIsWithinSVGText) {
    flags |= ITEM_IS_WITHIN_SVG_TEXT;
  }
  if (aItemAllowsTextPathChild && aParentItem.mIsForSVGAElement) {
    flags |= ITEM_ALLOWS_TEXT_PATH_CHILD;
  }

  if (!aParentItem.mAnonChildren.IsEmpty()) {
    // Use the anon-children list instead of the content tree child list so
    // that we use any special style context that should be associated with
    // the children, and so that we won't try to construct grandchildren frame
    // constructor items before the frame is available for their parent.
    AddFCItemsForAnonymousContent(aState, nullptr, aParentItem.mAnonChildren,
                                  aParentItem.mChildItems, flags);
  } else {
    // Use the content tree child list:
    FlattenedChildIterator iter(parentContent);
    for (nsIContent* content = iter.GetNextChild(); content; content = iter.GetNextChild()) {
      // Get the parent of the content and check if it is a XBL children element
      // (if the content is a children element then contentParent != parentContent because the
      // FlattenedChildIterator will transitively iterate through <xbl:children>
      // for default content). Push the children element as an ancestor here because
      // it does not have a frame and would not otherwise be pushed as an ancestor.
      nsIContent* contentParent = content->GetParent();
      MOZ_ASSERT(contentParent, "Parent must be non-null because we are iterating children.");
      TreeMatchContext::AutoAncestorPusher insertionPointPusher(aState.mTreeMatchContext);
      if (contentParent != parentContent && contentParent->IsElement()) {
        if (aState.mTreeMatchContext.mAncestorFilter.HasFilter()) {
          insertionPointPusher.PushAncestorAndStyleScope(contentParent->AsElement());
        } else {
          insertionPointPusher.PushStyleScope(contentParent->AsElement());
        }
      }

      // Manually check for comments/PIs, since we don't have a frame to pass to
      // AddFrameConstructionItems.  We know our parent is a non-replaced inline,
      // so there is no need to do the NeedFrameFor check.
      content->UnsetFlags(NODE_DESCENDANTS_NEED_FRAMES | NODE_NEEDS_FRAME);
      if (content->IsNodeOfType(nsINode::eCOMMENT) ||
          content->IsNodeOfType(nsINode::ePROCESSING_INSTRUCTION)) {
        continue;
      }

      // See comment explaining why we need to remove the "is possible
      // restyle root" flags in AddFrameConstructionItems.  But note
      // that we can remove all restyle flags, just like in
      // ProcessChildren and for the same reason.
      content->UnsetRestyleFlagsIfGecko();

      RefPtr<nsStyleContext> childContext =
        ResolveStyleContext(parentStyleContext, content, &aState);

      AddFrameConstructionItemsInternal(aState, content, nullptr,
                                        content->NodeInfo()->NameAtom(),
                                        content->GetNameSpaceID(),
                                        iter.XBLInvolved(), childContext,
                                        flags, nullptr,
                                        aParentItem.mChildItems);
    }
  }

  if (!aItemIsWithinSVGText) {
    // Probe for generated content after
    CreateGeneratedContentItem(aState, nullptr, parentContent, parentStyleContext,
                               CSSPseudoElementType::after,
                               aParentItem.mChildItems);
  }

  aParentItem.mIsAllInline = aParentItem.mChildItems.AreAllItemsInline();
}

// return whether it's ok to append (in the AppendFrames sense) to
// aParentFrame if our nextSibling is aNextSibling.  aParentFrame must
// be an ib-split inline.
static bool
IsSafeToAppendToIBSplitInline(nsIFrame* aParentFrame, nsIFrame* aNextSibling)
{
  NS_PRECONDITION(IsInlineFrame(aParentFrame),
                  "Must have an inline parent here");
  do {
    NS_ASSERTION(IsFramePartOfIBSplit(aParentFrame),
                 "How is this not part of an ib-split?");
    if (aNextSibling || aParentFrame->GetNextContinuation() ||
        GetIBSplitSibling(aParentFrame)) {
      return false;
    }

    aNextSibling = aParentFrame->GetNextSibling();
    aParentFrame = aParentFrame->GetParent();
  } while (IsInlineFrame(aParentFrame));

  return true;
}

bool
nsCSSFrameConstructor::WipeContainingBlock(nsFrameConstructorState& aState,
                                           nsIFrame* aContainingBlock,
                                           nsIFrame* aFrame,
                                           FrameConstructionItemList& aItems,
                                           bool aIsAppend,
                                           nsIFrame* aPrevSibling)
{
  if (aItems.IsEmpty()) {
    return false;
  }

  // Before we go and append the frames, we must check for several
  // special situations.

  // Situation #1 is a XUL frame that contains frames that are required
  // to be wrapped in blocks.
  if (aFrame->IsXULBoxFrame() &&
      !(aFrame->GetStateBits() & NS_STATE_BOX_WRAPS_KIDS_IN_BLOCK) &&
      aItems.AnyItemsNeedBlockParent()) {
    RecreateFramesForContent(aFrame->GetContent(), true,
                             REMOVE_FOR_RECONSTRUCTION, nullptr);
    return true;
  }

  nsIFrame* nextSibling = ::GetInsertNextSibling(aFrame, aPrevSibling);

  // Situation #2 is a flex or grid container frame into which we're inserting
  // new inline non-replaced children, adjacent to an existing anonymous
  // flex or grid item.
  nsIAtom* frameType = aFrame->GetType();
  if (frameType == nsGkAtoms::flexContainerFrame ||
      frameType == nsGkAtoms::gridContainerFrame) {
    FCItemIterator iter(aItems);

    // Check if we're adding to-be-wrapped content right *after* an existing
    // anonymous flex or grid item (which would need to absorb this content).
    const bool isWebkitBox = IsFlexContainerForLegacyBox(aFrame, frameType);
    if (aPrevSibling && IsAnonymousFlexOrGridItem(aPrevSibling) &&
        iter.item().NeedsAnonFlexOrGridItem(aState, isWebkitBox)) {
      RecreateFramesForContent(aFrame->GetContent(), true,
                               REMOVE_FOR_RECONSTRUCTION, nullptr);
      return true;
    }

    // Check if we're adding to-be-wrapped content right *before* an existing
    // anonymous flex or grid item (which would need to absorb this content).
    if (nextSibling && IsAnonymousFlexOrGridItem(nextSibling)) {
      // Jump to the last entry in the list
      iter.SetToEnd();
      iter.Prev();
      if (iter.item().NeedsAnonFlexOrGridItem(aState, isWebkitBox)) {
        RecreateFramesForContent(aFrame->GetContent(), true,
                                 REMOVE_FOR_RECONSTRUCTION, nullptr);
        return true;
      }
    }
  }

  // Situation #3 is an anonymous flex or grid item that's getting new children
  // who don't want to be wrapped.
  if (IsAnonymousFlexOrGridItem(aFrame)) {
    AssertAnonymousFlexOrGridItemParent(aFrame, aFrame->GetParent());

    // We need to push a null float containing block to be sure that
    // "NeedsAnonFlexOrGridItem" will know we're not honoring floats for this
    // inserted content. (In particular, this is necessary in order for
    // its "GetGeometricParent" call to return the correct result.)
    // We're not honoring floats on this content because it has the
    // _flex/grid container_ as its parent in the content tree.
    nsFrameConstructorSaveState floatSaveState;
    aState.PushFloatContainingBlock(nullptr, floatSaveState);

    FCItemIterator iter(aItems);
    // Skip over things that _do_ need an anonymous flex item, because
    // they're perfectly happy to go here -- they won't cause a reframe.
    nsIFrame* containerFrame = aFrame->GetParent();
    const bool isWebkitBox =
      IsFlexContainerForLegacyBox(containerFrame, containerFrame->GetType());
    if (!iter.SkipItemsThatNeedAnonFlexOrGridItem(aState,
                                                  isWebkitBox)) {
      // We hit something that _doesn't_ need an anonymous flex item!
      // Rebuild the flex container to bust it out.
      RecreateFramesForContent(containerFrame->GetContent(), true,
                               REMOVE_FOR_RECONSTRUCTION, nullptr);
      return true;
    }

    // If we get here, then everything in |aItems| needs to be wrapped in
    // an anonymous flex or grid item.  That's where it's already going - good!
  }

  // Situation #4 is a ruby-related frame that's getting new children.
  // The situation for ruby is complex, especially when interacting with
  // spaces. It containes these two special cases apart from tables:
  // 1) There are effectively three types of white spaces in ruby frames
  //    we handle differently: leading/tailing/inter-level space,
  //    inter-base/inter-annotation space, and inter-segment space.
  //    These three types of spaces can be converted to each other when
  //    their sibling changes.
  // 2) The first effective child of a ruby frame must always be a ruby
  //    base container. It should be created or destroyed accordingly.
  if (IsRubyPseudo(aFrame) ||
      frameType == nsGkAtoms::rubyFrame ||
      RubyUtils::IsRubyContainerBox(frameType)) {
    // We want to optimize it better, and avoid reframing as much as
    // possible. But given the cases above, and the fact that a ruby
    // usually won't be very large, it should be fine to reframe it.
    RecreateFramesForContent(aFrame->GetContent(), true,
                             REMOVE_FOR_RECONSTRUCTION, nullptr);
    return true;
  }

  // Situation #5 is a case when table pseudo-frames don't work out right
  ParentType parentType = GetParentType(aFrame);
  // If all the kids want a parent of the type that aFrame is, then we're all
  // set to go.  Indeed, there won't be any table pseudo-frames created between
  // aFrame and the kids, so those won't need to be merged with any table
  // pseudo-frames that might already be kids of aFrame.  If aFrame itself is a
  // table pseudo-frame, then all the kids in this list would have wanted a
  // frame of that type wrapping them anyway, so putting them inside it is ok.
  if (!aItems.AllWantParentType(parentType)) {
    // Don't give up yet.  If parentType is not eTypeBlock and the parent is
    // not a generated content frame, then try filtering whitespace out of the
    // list.
    if (parentType != eTypeBlock && !aFrame->IsGeneratedContentFrame()) {
      // For leading whitespace followed by a kid that wants our parent type,
      // there are four cases:
      // 1) We have a previous sibling which is not a table pseudo.  That means
      //    that previous sibling wanted a (non-block) parent of the type we're
      //    looking at.  Then the whitespace comes between two table-internal
      //    elements, so should be collapsed out.
      // 2) We have a previous sibling which is a table pseudo.  It might have
      //    kids who want this whitespace, so we need to reframe.
      // 3) We have no previous sibling and our parent frame is not a table
      //    pseudo.  That means that we'll be at the beginning of our actual
      //    non-block-type parent, and the whitespace is OK to collapse out.
      //    If something is ever inserted before us, it'll find our own parent
      //    as its parent and if it's something that would care about the
      //    whitespace it'll want a block parent, so it'll trigger a reframe at
      //    that point.
      // 4) We have no previous sibling and our parent frame is a table pseudo.
      //    Need to reframe.
      // All that is predicated on finding the correct previous sibling.  We
      // might have to walk backwards along continuations from aFrame to do so.
      //
      // It's always OK to drop whitespace between any two items that want a
      // parent of type parentType.
      //
      // For trailing whitespace preceded by a kid that wants our parent type,
      // there are four cases:
      // 1) We have a next sibling which is not a table pseudo.  That means
      //    that next sibling wanted a (non-block) parent of the type we're
      //    looking at.  Then the whitespace comes between two table-internal
      //    elements, so should be collapsed out.
      // 2) We have a next sibling which is a table pseudo.  It might have
      //    kids who want this whitespace, so we need to reframe.
      // 3) We have no next sibling and our parent frame is not a table
      //    pseudo.  That means that we'll be at the end of our actual
      //    non-block-type parent, and the whitespace is OK to collapse out.
      //    If something is ever inserted after us, it'll find our own parent
      //    as its parent and if it's something that would care about the
      //    whitespace it'll want a block parent, so it'll trigger a reframe at
      //    that point.
      // 4) We have no next sibling and our parent frame is a table pseudo.
      //    Need to reframe.
      // All that is predicated on finding the correct next sibling.  We might
      // have to walk forward along continuations from aFrame to do so.  That
      // said, in the case when nextSibling is null at this point and aIsAppend
      // is true, we know we're in case 3.  Furthermore, in that case we don't
      // even have to worry about the table pseudo situation; we know our
      // parent is not a table pseudo there.
      FCItemIterator iter(aItems);
      FCItemIterator start(iter);
      do {
        if (iter.SkipItemsWantingParentType(parentType)) {
          break;
        }

        // iter points to an item that wants a different parent.  If it's not
        // whitespace, we're done; no more point scanning the list.
        if (!iter.item().IsWhitespace(aState)) {
          break;
        }

        if (iter == start) {
          // Leading whitespace.  How to handle this depends on our
          // previous sibling and aFrame.  See the long comment above.
          nsIFrame* prevSibling = aPrevSibling;
          if (!prevSibling) {
            // Try to find one after all
            nsIFrame* parentPrevCont = aFrame->GetPrevContinuation();
            while (parentPrevCont) {
              prevSibling = parentPrevCont->GetChildList(kPrincipalList).LastChild();
              if (prevSibling) {
                break;
              }
              parentPrevCont = parentPrevCont->GetPrevContinuation();
            }
          };
          if (prevSibling) {
            if (IsTablePseudo(prevSibling)) {
              // need to reframe
              break;
            }
          } else if (IsTablePseudo(aFrame)) {
            // need to reframe
            break;
          }
        }

        FCItemIterator spaceEndIter(iter);
        // Advance spaceEndIter past any whitespace
        bool trailingSpaces = spaceEndIter.SkipWhitespace(aState);

        bool okToDrop;
        if (trailingSpaces) {
          // Trailing whitespace.  How to handle this depeds on aIsAppend, our
          // next sibling and aFrame.  See the long comment above.
          okToDrop = aIsAppend && !nextSibling;
          if (!okToDrop) {
            if (!nextSibling) {
              // Try to find one after all
              nsIFrame* parentNextCont = aFrame->GetNextContinuation();
              while (parentNextCont) {
                nextSibling = parentNextCont->PrincipalChildList().FirstChild();
                if (nextSibling) {
                  break;
                }
                parentNextCont = parentNextCont->GetNextContinuation();
              }
            }

            okToDrop = (nextSibling && !IsTablePseudo(nextSibling)) ||
                       (!nextSibling && !IsTablePseudo(aFrame));
          }
#ifdef DEBUG
          else {
            NS_ASSERTION(!IsTablePseudo(aFrame), "How did that happen?");
          }
#endif
        } else {
          okToDrop = (spaceEndIter.item().DesiredParentType() == parentType);
        }

        if (okToDrop) {
          iter.DeleteItemsTo(spaceEndIter);
        } else {
          // We're done: we don't want to drop the whitespace, and it has the
          // wrong parent type.
          break;
        }

        // Now loop, since |iter| points to item right after the whitespace we
        // removed.
      } while (!iter.IsDone());
    }

    // We might be able to figure out some sort of optimizations here, but they
    // would have to depend on having a correct aPrevSibling and a correct next
    // sibling.  For example, we can probably avoid reframing if none of
    // aFrame, aPrevSibling, and next sibling are table pseudo-frames.  But it
    // doesn't seem worth it to worry about that for now, especially since we
    // in fact do not have a reliable aPrevSibling, nor any next sibling, in
    // this method.

    // aItems might have changed, so recheck the parent type thing.  In fact,
    // it might be empty, so recheck that too.
    if (aItems.IsEmpty()) {
      return false;
    }

    if (!aItems.AllWantParentType(parentType)) {
      // Reframing aFrame->GetContent() is good enough, since the content of
      // table pseudo-frames is the ancestor content.
      RecreateFramesForContent(aFrame->GetContent(), true,
                               REMOVE_FOR_RECONSTRUCTION, nullptr);
      return true;
    }
  }

  // Now we have several cases involving {ib} splits.  Put them all in a
  // do/while with breaks to take us to the "go and reconstruct" code.
  do {
    if (IsInlineFrame(aFrame)) {
      if (aItems.AreAllItemsInline()) {
        // We can just put the kids in.
        return false;
      }

      if (!IsFramePartOfIBSplit(aFrame)) {
        // Need to go ahead and reconstruct.
        break;
      }

      // Now we're adding kids including some blocks to an inline part of an
      // {ib} split.  If we plan to call AppendFrames, and don't have a next
      // sibling for the new frames, and our parent is the last continuation of
      // the last part of the {ib} split, and the same is true of all our
      // ancestor inlines (they have no following continuations and they're the
      // last part of their {ib} splits and we'd be adding to the end for all
      // of them), then AppendFrames will handle things for us.  Bail out in
      // that case.
      if (aIsAppend && IsSafeToAppendToIBSplitInline(aFrame, nextSibling)) {
        return false;
      }

      // Need to reconstruct.
      break;
    }

    // Now we know we have a block parent.  If it's not part of an
    // ib-split, we're all set.
    if (!IsFramePartOfIBSplit(aFrame)) {
      return false;
    }

    // We're adding some kids to a block part of an {ib} split.  If all the
    // kids are blocks, we don't need to reconstruct.
    if (aItems.AreAllItemsBlock()) {
      return false;
    }

    // We might have some inline kids for this block.  Just fall out of the
    // loop and reconstruct.
  } while (0);

  // If we don't have a containing block, start with aFrame and look for one.
  if (!aContainingBlock) {
    aContainingBlock = aFrame;
  }

  // To find the right block to reframe, just walk up the tree until we find a
  // frame that is:
  // 1)  Not part of an IB split
  // 2)  Not a pseudo-frame
  // 3)  Not an inline frame
  // We're guaranteed to find one, since nsStyleContext::ApplyStyleFixups
  // enforces that the root is display:none, display:table, or display:block.
  // Note that walking up "too far" is OK in terms of correctness, even if it
  // might be a little inefficient.  This is why we walk out of all
  // pseudo-frames -- telling which ones are or are not OK to walk out of is
  // too hard (and I suspect that we do in fact need to walk out of all of
  // them).
  while (IsFramePartOfIBSplit(aContainingBlock) ||
         aContainingBlock->IsInlineOutside() ||
         aContainingBlock->StyleContext()->GetPseudo()) {
    aContainingBlock = aContainingBlock->GetParent();
    NS_ASSERTION(aContainingBlock,
                 "Must have non-inline, non-ib-split, non-pseudo frame as "
                 "root (or child of root, for a table root)!");
  }

  // Tell parent of the containing block to reformulate the
  // entire block. This is painful and definitely not optimal
  // but it will *always* get the right answer.

  nsIContent *blockContent = aContainingBlock->GetContent();
#ifdef DEBUG
  if (gNoisyContentUpdates) {
    printf("nsCSSFrameConstructor::WipeContainingBlock: blockContent=%p\n",
           static_cast<void*>(blockContent));
  }
#endif
  RecreateFramesForContent(blockContent, true, REMOVE_FOR_RECONSTRUCTION,
                           nullptr);
  return true;
}

nsresult
nsCSSFrameConstructor::ReframeContainingBlock(nsIFrame*    aFrame,
                                              RemoveFlags  aFlags,
                                              nsIContent** aDestroyedFramesFor)
{

#ifdef DEBUG
  // ReframeContainingBlock is a NASTY routine, it causes terrible performance problems
  // so I want to see when it is happening!  Unfortunately, it is happening way to often because
  // so much content on the web causes block-in-inline frame situations and we handle them
  // very poorly
  if (gNoisyContentUpdates) {
    printf("nsCSSFrameConstructor::ReframeContainingBlock frame=%p\n",
           static_cast<void*>(aFrame));
  }
#endif

  // XXXbz how exactly would we get here while isReflowing anyway?  Should this
  // whole test be ifdef DEBUG?
  if (mPresShell->IsReflowLocked()) {
    // don't ReframeContainingBlock, this will result in a crash
    // if we remove a tree that's in reflow - see bug 121368 for testcase
    NS_ERROR("Atemptted to nsCSSFrameConstructor::ReframeContainingBlock during a Reflow!!!");
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
#ifdef DEBUG
      if (gNoisyContentUpdates) {
        printf("  ==> blockContent=%p\n", static_cast<void*>(blockContent));
      }
#endif
      return RecreateFramesForContent(blockContent, true, aFlags, aDestroyedFramesFor);
    }
  }

  // If we get here, we're screwed!
  return RecreateFramesForContent(mPresShell->GetDocument()->GetRootElement(),
                                  true, aFlags, nullptr);
}

nsresult
nsCSSFrameConstructor::GenerateChildFrames(nsContainerFrame* aFrame)
{
  {
    nsAutoScriptBlocker scriptBlocker;
    BeginUpdate();

    nsFrameItems childItems;
    nsFrameConstructorState state(mPresShell, nullptr, nullptr, nullptr);
    // We don't have a parent frame with a pending binding constructor here,
    // so no need to worry about ordering of the kids' constructors with it.
    // Pass null for the PendingBinding.
    ProcessChildren(state, aFrame->GetContent(), aFrame->StyleContext(),
                    aFrame, false, childItems, false,
                    nullptr);

    aFrame->SetInitialChildList(kPrincipalList, childItems);

    EndUpdate();
  }

#ifdef ACCESSIBILITY
  nsAccessibilityService* accService = nsIPresShell::AccService();
  if (accService) {
    nsIContent* container = aFrame->GetContent();
    nsIContent* child = container->GetFirstChild();
    if (child) {
      accService->ContentRangeInserted(mPresShell, container, child, nullptr);
    }
  }
#endif

  // call XBL constructors after the frames are created
  mPresShell->GetDocument()->BindingManager()->ProcessAttachedQueue();

  return NS_OK;
}

//////////////////////////////////////////////////////////
// nsCSSFrameConstructor::FrameConstructionItem methods //
//////////////////////////////////////////////////////////
bool
nsCSSFrameConstructor::
FrameConstructionItem::IsWhitespace(nsFrameConstructorState& aState) const
{
  NS_PRECONDITION(aState.mCreatingExtraFrames ||
                  !mContent->GetPrimaryFrame(), "How did that happen?");
  if (!mIsText) {
    return false;
  }
  mContent->SetFlags(NS_CREATE_FRAME_IF_NON_WHITESPACE |
                     NS_REFRAME_IF_WHITESPACE);
  return mContent->TextIsOnlyWhitespace();
}

//////////////////////////////////////////////////////////////
// nsCSSFrameConstructor::FrameConstructionItemList methods //
//////////////////////////////////////////////////////////////
void
nsCSSFrameConstructor::FrameConstructionItemList::
AdjustCountsForItem(FrameConstructionItem* aItem, int32_t aDelta)
{
  NS_PRECONDITION(aDelta == 1 || aDelta == -1, "Unexpected delta");
  mItemCount += aDelta;
  if (aItem->mIsAllInline) {
    mInlineCount += aDelta;
  }
  if (aItem->mIsBlock) {
    mBlockCount += aDelta;
  }
  if (aItem->mIsLineParticipant) {
    mLineParticipantCount += aDelta;
  }
  mDesiredParentCounts[aItem->DesiredParentType()] += aDelta;
}

////////////////////////////////////////////////////////////////////////
// nsCSSFrameConstructor::FrameConstructionItemList::Iterator methods //
////////////////////////////////////////////////////////////////////////
inline bool
nsCSSFrameConstructor::FrameConstructionItemList::
Iterator::SkipItemsWantingParentType(ParentType aParentType)
{
  NS_PRECONDITION(!IsDone(), "Shouldn't be done yet");
  while (item().DesiredParentType() == aParentType) {
    Next();
    if (IsDone()) {
      return true;
    }
  }
  return false;
}

inline bool
nsCSSFrameConstructor::FrameConstructionItemList::
Iterator::SkipItemsNotWantingParentType(ParentType aParentType)
{
  NS_PRECONDITION(!IsDone(), "Shouldn't be done yet");
  while (item().DesiredParentType() != aParentType) {
    Next();
    if (IsDone()) {
      return true;
    }
  }
  return false;
}

// Note: we implement -webkit-box & -webkit-inline-box using
// nsFlexContainerFrame, but we use different rules for what gets wrapped in an
// anonymous flex item.
bool
nsCSSFrameConstructor::FrameConstructionItem::
  NeedsAnonFlexOrGridItem(const nsFrameConstructorState& aState,
                          bool aIsWebkitBox)
{
  if (mFCData->mBits & FCDATA_IS_LINE_PARTICIPANT) {
    // This will be an inline non-replaced box.
    return true;
  }

  if (aIsWebkitBox &&
      mStyleContext->StyleDisplay()->IsInlineOutsideStyle()) {
    // In a -webkit-box, all inline-level content gets wrapped in an anon item.
    return true;
  }

  return false;
}

inline bool
nsCSSFrameConstructor::FrameConstructionItemList::
Iterator::SkipItemsThatNeedAnonFlexOrGridItem(
  const nsFrameConstructorState& aState,
  bool aIsWebkitBox)
{
  NS_PRECONDITION(!IsDone(), "Shouldn't be done yet");
  while (item().NeedsAnonFlexOrGridItem(aState, aIsWebkitBox)) {
    Next();
    if (IsDone()) {
      return true;
    }
  }
  return false;
}

inline bool
nsCSSFrameConstructor::FrameConstructionItemList::
Iterator::SkipItemsThatDontNeedAnonFlexOrGridItem(
  const nsFrameConstructorState& aState,
  bool aIsWebkitBox)
{
  NS_PRECONDITION(!IsDone(), "Shouldn't be done yet");
  while (!(item().NeedsAnonFlexOrGridItem(aState, aIsWebkitBox))) {
    Next();
    if (IsDone()) {
      return true;
    }
  }
  return false;
}

inline bool
nsCSSFrameConstructor::FrameConstructionItemList::
Iterator::SkipItemsNotWantingRubyParent()
{
  NS_PRECONDITION(!IsDone(), "Shouldn't be done yet");
  while (!IsRubyParentType(item().DesiredParentType())) {
    Next();
    if (IsDone()) {
      return true;
    }
  }
  return false;
}

inline bool
nsCSSFrameConstructor::FrameConstructionItemList::
Iterator::SkipWhitespace(nsFrameConstructorState& aState)
{
  NS_PRECONDITION(!IsDone(), "Shouldn't be done yet");
  NS_PRECONDITION(item().IsWhitespace(aState), "Not pointing to whitespace?");
  do {
    Next();
    if (IsDone()) {
      return true;
    }
  } while (item().IsWhitespace(aState));

  return false;
}

void
nsCSSFrameConstructor::FrameConstructionItemList::
Iterator::AppendItemToList(FrameConstructionItemList& aTargetList)
{
  NS_ASSERTION(&aTargetList != &mList, "Unexpected call");
  NS_PRECONDITION(!IsDone(), "should not be done");

  FrameConstructionItem* item = mCurrent;
  Next();
  item->remove();
  aTargetList.mItems.insertBack(item);

  mList.AdjustCountsForItem(item, -1);
  aTargetList.AdjustCountsForItem(item, 1);
}

void
nsCSSFrameConstructor::FrameConstructionItemList::
Iterator::AppendItemsToList(const Iterator& aEnd,
                            FrameConstructionItemList& aTargetList)
{
  NS_ASSERTION(&aTargetList != &mList, "Unexpected call");
  NS_PRECONDITION(&mList == &aEnd.mList, "End iterator for some other list?");

  // We can't just move our guts to the other list if it already has
  // some information or if we're not moving our entire list.
  if (!AtStart() || !aEnd.IsDone() || !aTargetList.IsEmpty() ||
      !aTargetList.mUndisplayedItems.IsEmpty()) {
    do {
      AppendItemToList(aTargetList);
    } while (*this != aEnd);
    return;
  }

  // Move our entire list of items into the empty target list.
  aTargetList.mItems = Move(mList.mItems);

  // Copy over the various counters
  aTargetList.mInlineCount = mList.mInlineCount;
  aTargetList.mBlockCount = mList.mBlockCount;
  aTargetList.mLineParticipantCount = mList.mLineParticipantCount;
  aTargetList.mItemCount = mList.mItemCount;
  memcpy(aTargetList.mDesiredParentCounts, mList.mDesiredParentCounts,
         sizeof(aTargetList.mDesiredParentCounts));

  // Swap out undisplayed item arrays, before we nuke the array on our end
  aTargetList.mUndisplayedItems.SwapElements(mList.mUndisplayedItems);

  // reset mList
  mList.~FrameConstructionItemList();
  new (&mList) FrameConstructionItemList();

  // Point ourselves to aEnd, as advertised
  SetToEnd();
  NS_POSTCONDITION(*this == aEnd, "How did that happen?");
}

void
nsCSSFrameConstructor::FrameConstructionItemList::
Iterator::InsertItem(FrameConstructionItem* aItem)
{
  if (IsDone()) {
    mList.mItems.insertBack(aItem);
  } else {
    // Just insert the item before us.  There's no magic here.
    mCurrent->setPrevious(aItem);
  }
  mList.AdjustCountsForItem(aItem, 1);

  NS_POSTCONDITION(aItem->getNext() == mCurrent, "How did that happen?");
}

void
nsCSSFrameConstructor::FrameConstructionItemList::
Iterator::DeleteItemsTo(const Iterator& aEnd)
{
  NS_PRECONDITION(&mList == &aEnd.mList, "End iterator for some other list?");
  NS_PRECONDITION(*this != aEnd, "Shouldn't be at aEnd yet");

  do {
    NS_ASSERTION(!IsDone(), "Ran off end of list?");
    FrameConstructionItem* item = mCurrent;
    Next();
    item->remove();
    mList.AdjustCountsForItem(item, -1);
    delete item;
  } while (*this != aEnd);
}
