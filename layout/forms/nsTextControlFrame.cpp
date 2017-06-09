/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "gfxContext.h"
#include "nsCOMPtr.h"
#include "nsFontMetrics.h"
#include "nsTextControlFrame.h"
#include "nsIPlaintextEditor.h"
#include "nsCaret.h"
#include "nsCSSPseudoElements.h"
#include "nsGenericHTMLElement.h"
#include "nsIEditor.h"
#include "nsTextFragment.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsNameSpaceManager.h"
#include "nsFormControlFrame.h" //for registering accesskeys

#include "nsIContent.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsLayoutUtils.h"
#include "nsIDOMElement.h"
#include "nsIDOMHTMLElement.h"
#include "nsIPresShell.h"

#include <algorithm>
#include "nsIDOMNodeList.h" //for selection setting helper func
#include "nsIDOMRange.h" //for selection setting helper func
#include "nsPIDOMWindow.h" //needed for notify selection changed to update the menus ect.
#include "nsIDOMNode.h"

#include "nsIDOMText.h" //for multiline getselection
#include "nsFocusManager.h"
#include "nsPresState.h"
#include "nsContentList.h"
#include "nsAttrValueInlines.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/TextEditRules.h"
#include "nsContentUtils.h"
#include "nsTextNode.h"
#include "mozilla/StyleSetHandle.h"
#include "mozilla/StyleSetHandleInlines.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/HTMLTextAreaElement.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/MathAlgorithms.h"
#include "nsFrameSelection.h"

#define DEFAULT_COLUMN_WIDTH 20

using namespace mozilla;

nsIFrame*
NS_NewTextControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsTextControlFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsTextControlFrame)

NS_QUERYFRAME_HEAD(nsTextControlFrame)
  NS_QUERYFRAME_ENTRY(nsIFormControlFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
  NS_QUERYFRAME_ENTRY(nsITextControlFrame)
  NS_QUERYFRAME_ENTRY(nsIStatefulFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

#ifdef ACCESSIBILITY
a11y::AccType
nsTextControlFrame::AccessibleType()
{
  return a11y::eHTMLTextFieldType;
}
#endif

#ifdef DEBUG
class EditorInitializerEntryTracker {
public:
  explicit EditorInitializerEntryTracker(nsTextControlFrame &frame)
    : mFrame(frame)
    , mFirstEntry(false)
  {
    if (!mFrame.mInEditorInitialization) {
      mFrame.mInEditorInitialization = true;
      mFirstEntry = true;
    }
  }
  ~EditorInitializerEntryTracker()
  {
    if (mFirstEntry) {
      mFrame.mInEditorInitialization = false;
    }
  }
  bool EnteredMoreThanOnce() const { return !mFirstEntry; }
private:
  nsTextControlFrame &mFrame;
  bool mFirstEntry;
};
#endif

nsTextControlFrame::nsTextControlFrame(nsStyleContext* aContext)
  : nsContainerFrame(aContext, kClassID)
  , mFirstBaseline(NS_INTRINSIC_WIDTH_UNKNOWN)
  , mEditorHasBeenInitialized(false)
  , mIsProcessing(false)
  , mUsePlaceholder(false)
  , mUsePreview(false)
#ifdef DEBUG
  , mInEditorInitialization(false)
#endif
{
}

nsTextControlFrame::~nsTextControlFrame()
{
}

void
nsTextControlFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  mScrollEvent.Revoke();

  DeleteProperty(TextControlInitializer());

  // Unbind the text editor state object from the frame.  The editor will live
  // on, but things like controllers will be released.
  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element");
  txtCtrl->UnbindFromFrame(this);

  nsFormControlFrame::RegUnRegAccessKey(static_cast<nsIFrame*>(this), false);

  nsContainerFrame::DestroyFrom(aDestructRoot);
}

LogicalSize
nsTextControlFrame::CalcIntrinsicSize(gfxContext* aRenderingContext,
                                      WritingMode aWM,
                                      float aFontSizeInflation) const
{
  LogicalSize intrinsicSize(aWM);
  // Get leading and the Average/MaxAdvance char width 
  nscoord lineHeight  = 0;
  nscoord charWidth   = 0;
  nscoord charMaxAdvance  = 0;

  RefPtr<nsFontMetrics> fontMet =
    nsLayoutUtils::GetFontMetricsForFrame(this, aFontSizeInflation);

  lineHeight =
    ReflowInput::CalcLineHeight(GetContent(), StyleContext(),
                                      NS_AUTOHEIGHT, aFontSizeInflation);
  charWidth = fontMet->AveCharWidth();
  charMaxAdvance = fontMet->MaxAdvance();

  // Set the width equal to the width in characters
  int32_t cols = GetCols();
  intrinsicSize.ISize(aWM) = cols * charWidth;

  // To better match IE, take the maximum character width(in twips) and remove
  // 4 pixels add this on as additional padding(internalPadding). But only do
  // this if we think we have a fixed-width font.
  if (mozilla::Abs(charWidth - charMaxAdvance) > (unsigned)nsPresContext::CSSPixelsToAppUnits(1)) {
    nscoord internalPadding =
      std::max(0, charMaxAdvance - nsPresContext::CSSPixelsToAppUnits(4));
    nscoord t = nsPresContext::CSSPixelsToAppUnits(1); 
   // Round to a multiple of t
    nscoord rest = internalPadding % t; 
    if (rest < t - rest) {
      internalPadding -= rest;
    } else {
      internalPadding += t - rest;
    }
    // Now add the extra padding on (so that small input sizes work well)
    intrinsicSize.ISize(aWM) += internalPadding;
  } else {
    // This is to account for the anonymous <br> having a 1 twip width
    // in Full Standards mode, see BRFrame::Reflow and bug 228752.
    if (PresContext()->CompatibilityMode() == eCompatibility_FullStandards) {
      intrinsicSize.ISize(aWM) += 1;
    }
  }

  // Increment width with cols * letter-spacing.
  {
    const nsStyleCoord& lsCoord = StyleText()->mLetterSpacing;
    if (eStyleUnit_Coord == lsCoord.GetUnit()) {
      nscoord letterSpacing = lsCoord.GetCoordValue();
      if (letterSpacing != 0) {
        intrinsicSize.ISize(aWM) += cols * letterSpacing;
      }
    }
  }

  // Set the height equal to total number of rows (times the height of each
  // line, of course)
  intrinsicSize.BSize(aWM) = lineHeight * GetRows();

  // Add in the size of the scrollbars for textarea
  if (IsTextArea()) {
    nsIFrame* first = PrincipalChildList().FirstChild();

    nsIScrollableFrame *scrollableFrame = do_QueryFrame(first);
    NS_ASSERTION(scrollableFrame, "Child must be scrollable");

    if (scrollableFrame) {
      LogicalMargin scrollbarSizes(aWM,
        scrollableFrame->GetDesiredScrollbarSizes(PresContext(),
                                                  aRenderingContext));

      intrinsicSize.ISize(aWM) += scrollbarSizes.IStartEnd(aWM);
      intrinsicSize.BSize(aWM) += scrollbarSizes.BStartEnd(aWM);
    }
  }
  return intrinsicSize;
}

nsresult
nsTextControlFrame::EnsureEditorInitialized()
{
  // This method initializes our editor, if needed.

  // This code used to be called from CreateAnonymousContent(), but
  // when the editor set the initial string, it would trigger a
  // PresShell listener which called FlushPendingNotifications()
  // during frame construction. This was causing other form controls
  // to display wrong values.  Additionally, calling this every time
  // a text frame control is instantiated means that we're effectively
  // instantiating the editor for all text fields, even if they
  // never get used.  So, now this method is being called lazily only
  // when we actually need an editor.

  if (mEditorHasBeenInitialized)
    return NS_OK;

  nsIDocument* doc = mContent->GetComposedDoc();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  AutoWeakFrame weakFrame(this);

  // Flush out content on our document.  Have to do this, because script
  // blockers don't prevent the sink flushing out content and notifying in the
  // process, which can destroy frames.
  doc->FlushPendingNotifications(FlushType::ContentAndNotify);
  NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_ERROR_FAILURE);

  // Make sure that editor init doesn't do things that would kill us off
  // (especially off the script blockers it'll create for its DOM mutations).
  {
    nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
    MOZ_ASSERT(txtCtrl, "Content not a text control element");

    // Hide selection changes during the initialization, as webpages should not
    // be aware of these initializations
    AutoHideSelectionChanges hideSelectionChanges(txtCtrl->GetConstFrameSelection());

    nsAutoScriptBlocker scriptBlocker;

    // Time to mess with our security context... See comments in GetValue()
    // for why this is needed.
    mozilla::dom::AutoNoJSAPI nojsapi;

    // Make sure that we try to focus the content even if the method fails
    class EnsureSetFocus {
    public:
      explicit EnsureSetFocus(nsTextControlFrame* aFrame)
        : mFrame(aFrame) {}
      ~EnsureSetFocus() {
        if (nsContentUtils::IsFocusedContent(mFrame->GetContent()))
          mFrame->SetFocus(true, false);
      }
    private:
      nsTextControlFrame *mFrame;
    };
    EnsureSetFocus makeSureSetFocusHappens(this);

#ifdef DEBUG
    // Make sure we are not being called again until we're finished.
    // If reentrancy happens, just pretend that we don't have an editor.
    const EditorInitializerEntryTracker tracker(*this);
    NS_ASSERTION(!tracker.EnteredMoreThanOnce(),
                 "EnsureEditorInitialized has been called while a previous call was in progress");
#endif

    // Create an editor for the frame, if one doesn't already exist
    nsresult rv = txtCtrl->CreateEditor();
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_STATE(weakFrame.IsAlive());

    // Set mEditorHasBeenInitialized so that subsequent calls will use the
    // editor.
    mEditorHasBeenInitialized = true;

    if (weakFrame.IsAlive()) {
      uint32_t position = 0;

      // Set the selection to the end of the text field (bug 1287655),
      // but only if the contents has changed (bug 1337392).
      if (txtCtrl->ValueChanged()) {
        nsAutoString val;
        txtCtrl->GetTextEditorValue(val, true);
        position = val.Length();
      }

      SetSelectionEndPoints(position, position);
    }
  }
  NS_ENSURE_STATE(weakFrame.IsAlive());
  return NS_OK;
}

nsresult
nsTextControlFrame::CreateAnonymousContent(nsTArray<ContentInfo>& aElements)
{
  NS_ASSERTION(mContent, "We should have a content!");

  mState |= NS_FRAME_INDEPENDENT_SELECTION;

  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element");

  // Bind the frame to its text control
  nsresult rv = txtCtrl->BindToFrame(this);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIContent* rootNode = txtCtrl->GetRootEditorNode();
  NS_ENSURE_TRUE(rootNode, NS_ERROR_OUT_OF_MEMORY);

  if (!aElements.AppendElement(rootNode))
    return NS_ERROR_OUT_OF_MEMORY;

  // Do we need a placeholder node?
  nsAutoString placeholderTxt;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::placeholder,
                    placeholderTxt);
  nsContentUtils::RemoveNewlines(placeholderTxt);
  mUsePlaceholder = !placeholderTxt.IsEmpty();

  // Create the placeholder anonymous content if needed.
  if (mUsePlaceholder) {
    Element* placeholderNode = txtCtrl->CreatePlaceholderNode();
    NS_ENSURE_TRUE(placeholderNode, NS_ERROR_OUT_OF_MEMORY);

    // Associate ::placeholder pseudo-element with the placeholder node.
    placeholderNode->SetPseudoElementType(CSSPseudoElementType::placeholder);
    aElements.AppendElement(placeholderNode);

    if (!IsSingleLineTextControl()) {
      // For textareas, UpdateValueDisplay doesn't initialize the visibility
      // status of the placeholder because it returns early, so we have to
      // do that manually here.
      txtCtrl->UpdateOverlayTextVisibility(true);
    }
  }

  mUsePreview = txtCtrl->IsPreviewEnabled();

  if (mUsePreview) {
    // Create the preview anonymous content if needed.
    Element* previewNode = txtCtrl->CreatePreviewNode();
    NS_ENSURE_TRUE(previewNode, NS_ERROR_OUT_OF_MEMORY);

    aElements.AppendElement(previewNode);
  }

  rv = UpdateValueDisplay(false);
  NS_ENSURE_SUCCESS(rv, rv);

  // textareas are eagerly initialized
  bool initEagerly = !IsSingleLineTextControl();
  if (!initEagerly) {
    // Also, input elements which have a cached selection should get eager
    // editor initialization.
    nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
    NS_ASSERTION(txtCtrl, "Content not a text control element");
    initEagerly = txtCtrl->HasCachedSelection();
  }
  if (!initEagerly) {
    nsCOMPtr<nsIDOMHTMLElement> element = do_QueryInterface(txtCtrl);
    if (element) {
      // so are input text controls with spellcheck=true
      element->GetSpellcheck(&initEagerly);
    }
  }

  if (initEagerly) {
    NS_ASSERTION(!nsContentUtils::IsSafeToRunScript(),
                 "Someone forgot a script blocker?");
    EditorInitializer* initializer = new EditorInitializer(this);
    SetProperty(TextControlInitializer(), initializer);
    nsContentUtils::AddScriptRunner(initializer);
  }

  return NS_OK;
}

void
nsTextControlFrame::AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                             uint32_t aFilter)
{
  // This can be called off-main-thread during Servo traversal, so we take care
  // to avoid QI-ing the DOM node.
  nsITextControlElement* txtCtrl = nullptr;
  nsIContent* content = GetContent();
  if (content->IsHTMLElement(nsGkAtoms::input)) {
    txtCtrl = static_cast<HTMLInputElement*>(content);
  } else if (content->IsHTMLElement(nsGkAtoms::textarea)) {
    txtCtrl = static_cast<HTMLTextAreaElement*>(content);
  } else {
    MOZ_CRASH("Unexpected content type for nsTextControlFrame");
  }

  nsIContent* root = txtCtrl->GetRootEditorNode();
  if (root) {
    aElements.AppendElement(root);
  }

  nsIContent* placeholder = txtCtrl->GetPlaceholderNode();
  if (placeholder && !(aFilter & nsIContent::eSkipPlaceholderContent)) {
    aElements.AppendElement(placeholder);
  }

  nsIContent* preview = txtCtrl->GetPreviewNode();
  if (preview) {
    aElements.AppendElement(preview);
  }
}

nscoord
nsTextControlFrame::GetPrefISize(gfxContext* aRenderingContext)
{
  nscoord result = 0;
  DISPLAY_PREF_WIDTH(this, result);
  float inflation = nsLayoutUtils::FontSizeInflationFor(this);
  WritingMode wm = GetWritingMode();
  result = CalcIntrinsicSize(aRenderingContext, wm, inflation).ISize(wm);
  return result;
}

nscoord
nsTextControlFrame::GetMinISize(gfxContext* aRenderingContext)
{
  // Our min width is just our preferred width if we have auto width.
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);
  result = GetPrefISize(aRenderingContext);
  return result;
}

LogicalSize
nsTextControlFrame::ComputeAutoSize(gfxContext*         aRenderingContext,
                                    WritingMode         aWM,
                                    const LogicalSize&  aCBSize,
                                    nscoord             aAvailableISize,
                                    const LogicalSize&  aMargin,
                                    const LogicalSize&  aBorder,
                                    const LogicalSize&  aPadding,
                                    ComputeSizeFlags    aFlags)
{
  float inflation = nsLayoutUtils::FontSizeInflationFor(this);
  LogicalSize autoSize = CalcIntrinsicSize(aRenderingContext, aWM, inflation);

  // Note: nsContainerFrame::ComputeAutoSize only computes the inline-size (and
  // only for 'auto'), the block-size it returns is always NS_UNCONSTRAINEDSIZE.
  const nsStyleCoord& iSizeCoord = StylePosition()->ISize(aWM);
  if (iSizeCoord.GetUnit() == eStyleUnit_Auto) {
    if (aFlags & ComputeSizeFlags::eIClampMarginBoxMinSize) {
      // CalcIntrinsicSize isn't aware of grid-item margin-box clamping, so we
      // fall back to nsContainerFrame's ComputeAutoSize to handle that.
      // XXX maybe a font-inflation issue here? (per the assertion below).
      autoSize.ISize(aWM) =
        nsContainerFrame::ComputeAutoSize(aRenderingContext, aWM,
                                          aCBSize, aAvailableISize,
                                          aMargin, aBorder,
                                          aPadding, aFlags).ISize(aWM);
    }
#ifdef DEBUG
    else {
      LogicalSize ancestorAutoSize =
        nsContainerFrame::ComputeAutoSize(aRenderingContext, aWM,
                                          aCBSize, aAvailableISize,
                                          aMargin, aBorder,
                                          aPadding, aFlags);
      // Disabled when there's inflation; see comment in GetXULPrefSize.
      MOZ_ASSERT(inflation != 1.0f ||
                 ancestorAutoSize.ISize(aWM) == autoSize.ISize(aWM),
                 "Incorrect size computed by ComputeAutoSize?");
    }
#endif
  }
  return autoSize;
}

void
nsTextControlFrame::Reflow(nsPresContext*   aPresContext,
                           ReflowOutput&     aDesiredSize,
                           const ReflowInput& aReflowInput,
                           nsReflowStatus&          aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsTextControlFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);

  // make sure that the form registers itself on the initial/first reflow
  if (mState & NS_FRAME_FIRST_REFLOW) {
    nsFormControlFrame::RegUnRegAccessKey(this, true);
  }

  // set values of reflow's out parameters
  WritingMode wm = aReflowInput.GetWritingMode();
  LogicalSize
    finalSize(wm,
              aReflowInput.ComputedISize() +
              aReflowInput.ComputedLogicalBorderPadding().IStartEnd(wm),
              aReflowInput.ComputedBSize() +
              aReflowInput.ComputedLogicalBorderPadding().BStartEnd(wm));
  aDesiredSize.SetSize(wm, finalSize);

  // Calculate the baseline and store it in mFirstBaseline.
  nscoord lineHeight = aReflowInput.ComputedBSize();
  float inflation = nsLayoutUtils::FontSizeInflationFor(this);
  if (!IsSingleLineTextControl()) {
    lineHeight = ReflowInput::CalcLineHeight(GetContent(), StyleContext(),
                                             NS_AUTOHEIGHT, inflation);
  }
  RefPtr<nsFontMetrics> fontMet =
    nsLayoutUtils::GetFontMetricsForFrame(this, inflation);
  mFirstBaseline =
    nsLayoutUtils::GetCenteredFontBaseline(fontMet, lineHeight,
                                           wm.IsLineInverted()) +
    aReflowInput.ComputedLogicalBorderPadding().BStart(wm);
  aDesiredSize.SetBlockStartAscent(mFirstBaseline);

  // overflow handling
  aDesiredSize.SetOverflowAreasToDesiredBounds();
  // perform reflow on all kids
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    ReflowTextControlChild(kid, aPresContext, aReflowInput, aStatus, aDesiredSize);
    kid = kid->GetNextSibling();
  }

  // take into account css properties that affect overflow handling
  FinishAndStoreOverflow(&aDesiredSize);

  aStatus.Reset();
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aDesiredSize);
}

void
nsTextControlFrame::ReflowTextControlChild(nsIFrame*                aKid,
                                           nsPresContext*           aPresContext,
                                           const ReflowInput& aReflowInput,
                                           nsReflowStatus&          aStatus,
                                           ReflowOutput& aParentDesiredSize)
{
  // compute available size and frame offsets for child
  WritingMode wm = aKid->GetWritingMode();
  LogicalSize availSize = aReflowInput.ComputedSizeWithPadding(wm);
  availSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;

  ReflowInput kidReflowInput(aPresContext, aReflowInput, 
                                   aKid, availSize, nullptr,
                                   ReflowInput::CALLER_WILL_INIT);
  // Override padding with our computed padding in case we got it from theming or percentage
  kidReflowInput.Init(aPresContext, nullptr, nullptr, &aReflowInput.ComputedPhysicalPadding());

  // Set computed width and computed height for the child
  kidReflowInput.SetComputedWidth(aReflowInput.ComputedWidth());
  kidReflowInput.SetComputedHeight(aReflowInput.ComputedHeight());

  // Offset the frame by the size of the parent's border
  nscoord xOffset = aReflowInput.ComputedPhysicalBorderPadding().left -
                    aReflowInput.ComputedPhysicalPadding().left;
  nscoord yOffset = aReflowInput.ComputedPhysicalBorderPadding().top -
                    aReflowInput.ComputedPhysicalPadding().top;

  // reflow the child
  ReflowOutput desiredSize(aReflowInput);
  ReflowChild(aKid, aPresContext, desiredSize, kidReflowInput, 
              xOffset, yOffset, 0, aStatus);

  // place the child
  FinishReflowChild(aKid, aPresContext, desiredSize,
                    &kidReflowInput, xOffset, yOffset, 0);

  // consider the overflow
  aParentDesiredSize.mOverflowAreas.UnionWith(desiredSize.mOverflowAreas);
}

nsSize
nsTextControlFrame::GetXULMinSize(nsBoxLayoutState& aState)
{
  // XXXbz why?  Why not the nsBoxFrame sizes?
  return nsBox::GetXULMinSize(aState);
}

bool
nsTextControlFrame::IsXULCollapsed()
{
  // We're never collapsed in the box sense.
  return false;
}

NS_IMETHODIMP
nsTextControlFrame::ScrollOnFocusEvent::Run()
{
  if (mFrame) {
    nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(mFrame->GetContent());
    NS_ASSERTION(txtCtrl, "Content not a text control element");
    nsISelectionController* selCon = txtCtrl->GetSelectionController();
    if (selCon) {
      mFrame->mScrollEvent.Forget();
      selCon->ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL,
                                      nsISelectionController::SELECTION_FOCUS_REGION,
                                      nsISelectionController::SCROLL_SYNCHRONOUS);
    }
  }
  return NS_OK;
}

//IMPLEMENTING NS_IFORMCONTROLFRAME
void nsTextControlFrame::SetFocus(bool aOn, bool aRepaint)
{
  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element");

  // Revoke the previous scroll event if one exists
  mScrollEvent.Revoke();

  // If 'dom.placeholeder.show_on_focus' preference is 'false', focusing or
  // blurring the frame can have an impact on the placeholder visibility.
  if (mUsePlaceholder) {
    txtCtrl->UpdateOverlayTextVisibility(true);
  }

  if (!aOn) {
    return;
  }

  nsISelectionController* selCon = txtCtrl->GetSelectionController();
  if (!selCon)
    return;

  nsCOMPtr<nsISelection> ourSel;
  selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, 
    getter_AddRefs(ourSel));
  if (!ourSel) return;

  nsIPresShell* presShell = PresContext()->GetPresShell();
  RefPtr<nsCaret> caret = presShell->GetCaret();
  if (!caret) return;

  // Scroll the current selection into view
  nsISelection *caretSelection = caret->GetSelection();
  const bool isFocusedRightNow = ourSel == caretSelection;
  if (!isFocusedRightNow) {
    // Don't scroll the current selection if we've been focused using the mouse.
    uint32_t lastFocusMethod = 0;
    nsIDocument* doc = GetContent()->GetComposedDoc();
    if (doc) {
      nsIFocusManager* fm = nsFocusManager::GetFocusManager();
      if (fm) {
        fm->GetLastFocusMethod(doc->GetWindow(), &lastFocusMethod);
      }
    }
    if (!(lastFocusMethod & nsIFocusManager::FLAG_BYMOUSE)) {
      RefPtr<ScrollOnFocusEvent> event = new ScrollOnFocusEvent(this);
      nsresult rv = mContent->OwnerDoc()->Dispatch("ScrollOnFocusEvent",
                                                   TaskCategory::Other,
                                                   do_AddRef(event));
      if (NS_SUCCEEDED(rv)) {
        mScrollEvent = event;
      }
    }
  }

  // tell the caret to use our selection
  caret->SetSelection(ourSel);

  // mutual-exclusion: the selection is either controlled by the
  // document or by the text input/area. Clear any selection in the
  // document since the focus is now on our independent selection.

  nsCOMPtr<nsISelectionController> selcon = do_QueryInterface(presShell);
  nsCOMPtr<nsISelection> docSel;
  selcon->GetSelection(nsISelectionController::SELECTION_NORMAL,
    getter_AddRefs(docSel));
  if (!docSel) return;

  bool isCollapsed = false;
  docSel->GetIsCollapsed(&isCollapsed);
  if (!isCollapsed)
    docSel->RemoveAllRanges();
}

nsresult nsTextControlFrame::SetFormProperty(nsIAtom* aName, const nsAString& aValue)
{
  if (!mIsProcessing)//some kind of lock.
  {
    mIsProcessing = true;
    if (nsGkAtoms::select == aName)
    {
      // Select all the text.
      //
      // XXX: This is lame, we can't call editor's SelectAll method
      //      because that triggers AutoCopies in unix builds.
      //      Instead, we have to call our own homegrown version
      //      of select all which merely builds a range that selects
      //      all of the content and adds that to the selection.

      AutoWeakFrame weakThis = this;
      SelectAllOrCollapseToEndOfText(true);  // NOTE: can destroy the world
      if (!weakThis.IsAlive()) {
        return NS_OK;
      }
    }
    mIsProcessing = false;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsTextControlFrame::GetEditor(nsIEditor **aEditor)
{
  NS_ENSURE_ARG_POINTER(aEditor);

  nsresult rv = EnsureEditorInitialized();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element");
  *aEditor = txtCtrl->GetTextEditor();
  NS_IF_ADDREF(*aEditor);
  return NS_OK;
}

nsresult
nsTextControlFrame::SetSelectionInternal(nsIDOMNode *aStartNode,
                                         uint32_t aStartOffset,
                                         nsIDOMNode *aEndNode,
                                         uint32_t aEndOffset,
                                         nsITextControlFrame::SelectionDirection aDirection)
{
  // Create a new range to represent the new selection.
  // Note that we use a new range to avoid having to do
  // isIncreasing checks to avoid possible errors.

  RefPtr<nsRange> range = new nsRange(mContent);
  // Be careful to use internal nsRange methods which do not check to make sure
  // we have access to the node.
  nsCOMPtr<nsINode> start = do_QueryInterface(aStartNode);
  nsCOMPtr<nsINode> end = do_QueryInterface(aEndNode);
  // XXXbz nsRange::SetStartAndEnd takes int32_t (and ranges generally work on
  // int32_t), but we're passing uint32_t.  The good news is that at this point
  // our endpoints should really be within our length, so not really that big.
  // And if they _are_ that big, SetStartAndEnd() will simply error out, which
  // is not too bad for a case we don't expect to happen.
  nsresult rv = range->SetStartAndEnd(start, aStartOffset, end, aEndOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the selection, clear it and add the new range to it!
  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element");
  nsISelectionController* selCon = txtCtrl->GetSelectionController();
  NS_ENSURE_TRUE(selCon, NS_ERROR_FAILURE);

  nsCOMPtr<nsISelection> selection;
  selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));  
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);

  nsCOMPtr<nsISelectionPrivate> selPriv = do_QueryInterface(selection, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsDirection direction;
  if (aDirection == eNone) {
    // Preserve the direction
    direction = selPriv->GetSelectionDirection();
  } else {
    direction = (aDirection == eBackward) ? eDirPrevious : eDirNext;
  }

  rv = selection->RemoveAllRanges();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = selection->AddRange(range);  // NOTE: can destroy the world
  NS_ENSURE_SUCCESS(rv, rv);

  selPriv->SetSelectionDirection(direction);
  return rv;
}

nsresult
nsTextControlFrame::ScrollSelectionIntoView()
{
  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element");
  nsISelectionController* selCon = txtCtrl->GetSelectionController();
  if (selCon) {
    // Scroll the selection into view (see bug 231389).
    return selCon->ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL,
                                           nsISelectionController::SELECTION_FOCUS_REGION,
                                           nsISelectionController::SCROLL_FIRST_ANCESTOR_ONLY);
  }

  return NS_ERROR_FAILURE;
}

nsresult
nsTextControlFrame::GetRootNodeAndInitializeEditor(nsIDOMElement **aRootElement)
{
  NS_ENSURE_ARG_POINTER(aRootElement);

  nsCOMPtr<nsIEditor> editor;
  GetEditor(getter_AddRefs(editor));
  if (!editor)
    return NS_OK;

  return editor->GetRootElement(aRootElement);
}

nsresult
nsTextControlFrame::SelectAllOrCollapseToEndOfText(bool aSelect)
{
  nsCOMPtr<nsIDOMElement> rootElement;
  nsresult rv = GetRootNodeAndInitializeEditor(getter_AddRefs(rootElement));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContent> rootContent = do_QueryInterface(rootElement);
  nsCOMPtr<nsIDOMNode> rootNode(do_QueryInterface(rootElement));

  NS_ENSURE_TRUE(rootNode && rootContent, NS_ERROR_FAILURE);

  int32_t numChildren = rootContent->GetChildCount();

  if (numChildren > 0) {
    // We never want to place the selection after the last
    // br under the root node!
    nsIContent *child = rootContent->GetChildAt(numChildren - 1);
    if (child) {
      if (child->IsHTMLElement(nsGkAtoms::br))
        --numChildren;
    }
    if (!aSelect && numChildren) {
      child = rootContent->GetChildAt(numChildren - 1);
      if (child && child->IsNodeOfType(nsINode::eTEXT)) {
        rootNode = do_QueryInterface(child);
        const nsTextFragment* fragment = child->GetText();
        numChildren = fragment ? fragment->GetLength() : 0;
      }
    }
  }

  rv = SetSelectionInternal(rootNode, aSelect ? 0 : numChildren,
                            rootNode, numChildren);
  NS_ENSURE_SUCCESS(rv, rv);

  return ScrollSelectionIntoView();
}

nsresult
nsTextControlFrame::SetSelectionEndPoints(uint32_t aSelStart, uint32_t aSelEnd,
                                          nsITextControlFrame::SelectionDirection aDirection)
{
  NS_ASSERTION(aSelStart <= aSelEnd, "Invalid selection offsets!");

  if (aSelStart > aSelEnd)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNode> startNode, endNode;
  uint32_t startOffset, endOffset;

  // Calculate the selection start point.

  nsresult rv = OffsetToDOMPoint(aSelStart, getter_AddRefs(startNode), &startOffset);

  NS_ENSURE_SUCCESS(rv, rv);

  if (aSelStart == aSelEnd) {
    // Collapsed selection, so start and end are the same!
    endNode   = startNode;
    endOffset = startOffset;
  }
  else {
    // Selection isn't collapsed so we have to calculate
    // the end point too.

    rv = OffsetToDOMPoint(aSelEnd, getter_AddRefs(endNode), &endOffset);

    NS_ENSURE_SUCCESS(rv, rv);
  }

  return SetSelectionInternal(startNode, startOffset, endNode, endOffset, aDirection);
}

NS_IMETHODIMP
nsTextControlFrame::SetSelectionRange(uint32_t aSelStart, uint32_t aSelEnd,
                                      nsITextControlFrame::SelectionDirection aDirection)
{
  nsresult rv = EnsureEditorInitialized();
  NS_ENSURE_SUCCESS(rv, rv);

  if (aSelStart > aSelEnd) {
    // Simulate what we'd see SetSelectionStart() was called, followed
    // by a SetSelectionEnd().

    aSelStart   = aSelEnd;
  }

  return SetSelectionEndPoints(aSelStart, aSelEnd, aDirection);
}


nsresult
nsTextControlFrame::OffsetToDOMPoint(uint32_t aOffset,
                                     nsIDOMNode** aResult,
                                     uint32_t* aPosition)
{
  NS_ENSURE_ARG_POINTER(aResult && aPosition);

  *aResult = nullptr;
  *aPosition = 0;

  nsCOMPtr<nsIDOMElement> rootElement;
  nsresult rv = GetRootNodeAndInitializeEditor(getter_AddRefs(rootElement));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIDOMNode> rootNode(do_QueryInterface(rootElement));

  NS_ENSURE_TRUE(rootNode, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMNodeList> nodeList;

  rv = rootNode->GetChildNodes(getter_AddRefs(nodeList));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(nodeList, NS_ERROR_FAILURE);

  uint32_t length = 0;

  rv = nodeList->GetLength(&length);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(length <= 2, "We should have one text node and one mozBR at most");

  nsCOMPtr<nsIDOMNode> firstNode;
  rv = nodeList->Item(0, getter_AddRefs(firstNode));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(firstNode);

  if (length == 0) {
    NS_IF_ADDREF(*aResult = rootNode);
    *aPosition = 0;
  } else if (textNode) {
    uint32_t textLength = 0;
    textNode->GetLength(&textLength);
    if (length == 2 && aOffset == textLength) {
      // If we're at the end of the text node and we have a trailing BR node,
      // set the selection on the BR node.
      NS_IF_ADDREF(*aResult = rootNode);
      *aPosition = 1;
    } else {
      // Otherwise, set the selection on the textnode itself.
      NS_IF_ADDREF(*aResult = firstNode);
      *aPosition = std::min(aOffset, textLength);
    }
  } else {
    NS_IF_ADDREF(*aResult = rootNode);
    *aPosition = 0;
  }

  return NS_OK;
}

/////END INTERFACE IMPLEMENTATIONS

////NSIFRAME
nsresult
nsTextControlFrame::AttributeChanged(int32_t         aNameSpaceID,
                                     nsIAtom*        aAttribute,
                                     int32_t         aModType)
{
  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element");
  nsISelectionController* selCon = txtCtrl->GetSelectionController();
  const bool needEditor = nsGkAtoms::maxlength == aAttribute ||
                            nsGkAtoms::readonly == aAttribute ||
                            nsGkAtoms::disabled == aAttribute ||
                            nsGkAtoms::spellcheck == aAttribute;
  nsCOMPtr<nsIEditor> editor;
  if (needEditor) {
    GetEditor(getter_AddRefs(editor));
  }
  if ((needEditor && !editor) || !selCon) {
    return nsContainerFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
  }

  if (nsGkAtoms::maxlength == aAttribute) {
    int32_t maxLength;
    bool maxDefined = GetMaxLength(&maxLength);
    nsCOMPtr<nsIPlaintextEditor> textEditor = do_QueryInterface(editor);
    if (textEditor) {
      if (maxDefined) { // set the maxLength attribute
        textEditor->SetMaxTextLength(maxLength);
        // if maxLength>docLength, we need to truncate the doc content
      } else { // unset the maxLength attribute
        textEditor->SetMaxTextLength(-1);
      }
    }
    return NS_OK;
  }

  if (nsGkAtoms::readonly == aAttribute) {
    uint32_t flags;
    editor->GetFlags(&flags);
    if (AttributeExists(nsGkAtoms::readonly)) { // set readonly
      flags |= nsIPlaintextEditor::eEditorReadonlyMask;
      if (nsContentUtils::IsFocusedContent(mContent)) {
        selCon->SetCaretEnabled(false);
      }
    } else { // unset readonly
      flags &= ~(nsIPlaintextEditor::eEditorReadonlyMask);
      if (!(flags & nsIPlaintextEditor::eEditorDisabledMask) &&
          nsContentUtils::IsFocusedContent(mContent)) {
        selCon->SetCaretEnabled(true);
      }
    }
    editor->SetFlags(flags);
    return NS_OK;
  }

  if (nsGkAtoms::disabled == aAttribute) {
    uint32_t flags;
    editor->GetFlags(&flags);
    int16_t displaySelection = nsISelectionController::SELECTION_OFF;
    const bool focused = nsContentUtils::IsFocusedContent(mContent);
    const bool hasAttr = AttributeExists(nsGkAtoms::disabled);
    if (hasAttr) { // set disabled
      flags |= nsIPlaintextEditor::eEditorDisabledMask;
    } else { // unset disabled
      flags &= ~(nsIPlaintextEditor::eEditorDisabledMask);
      displaySelection = focused ? nsISelectionController::SELECTION_ON
                                 : nsISelectionController::SELECTION_HIDDEN;
    }
    selCon->SetDisplaySelection(displaySelection);
    if (focused) {
      selCon->SetCaretEnabled(!hasAttr);
    }
    editor->SetFlags(flags);
    return NS_OK;
  }

  if (!mEditorHasBeenInitialized && nsGkAtoms::value == aAttribute) {
    UpdateValueDisplay(true);
    return NS_OK;
  }

  // Allow the base class to handle common attributes supported by all form
  // elements...
  return nsContainerFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
}


nsresult
nsTextControlFrame::GetText(nsString& aText)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element");
  if (IsSingleLineTextControl()) {
    // There will be no line breaks so we can ignore the wrap property.
    txtCtrl->GetTextEditorValue(aText, true);
  } else {
    nsCOMPtr<nsIDOMHTMLTextAreaElement> textArea = do_QueryInterface(mContent);
    if (textArea) {
      rv = textArea->GetValue(aText);
    }
  }
  return rv;
}


///END NSIFRAME OVERLOADS
/////BEGIN PROTECTED METHODS

bool
nsTextControlFrame::GetMaxLength(int32_t* aSize)
{
  *aSize = -1;

  nsGenericHTMLElement *content = nsGenericHTMLElement::FromContent(mContent);
  if (content) {
    const nsAttrValue* attr = content->GetParsedAttr(nsGkAtoms::maxlength);
    if (attr && attr->Type() == nsAttrValue::eInteger) {
      *aSize = attr->GetIntegerValue();

      return true;
    }
  }
  return false;
}

// END IMPLEMENTING NS_IFORMCONTROLFRAME

void
nsTextControlFrame::SetInitialChildList(ChildListID     aListID,
                                        nsFrameList&    aChildList)
{
  nsContainerFrame::SetInitialChildList(aListID, aChildList);
  if (aListID != kPrincipalList) {
    return;
  }

  // Mark the scroll frame as being a reflow root. This will allow
  // incremental reflows to be initiated at the scroll frame, rather
  // than descending from the root frame of the frame hierarchy.
  if (nsIFrame* first = PrincipalChildList().FirstChild()) {
    first->AddStateBits(NS_FRAME_REFLOW_ROOT);

    nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
    NS_ASSERTION(txtCtrl, "Content not a text control element");
    txtCtrl->InitializeKeyboardEventListeners();

    nsPoint* contentScrollPos = GetProperty(ContentScrollPos());
    if (contentScrollPos) {
      // If we have a scroll pos stored to be passed to our anonymous
      // div, do it here!
      nsIStatefulFrame* statefulFrame = do_QueryFrame(first);
      NS_ASSERTION(statefulFrame, "unexpected type of frame for the anonymous div");
      nsPresState fakePresState;
      fakePresState.SetScrollState(*contentScrollPos);
      statefulFrame->RestoreState(&fakePresState);
      RemoveProperty(ContentScrollPos());
      delete contentScrollPos;
    }
  }
}

void
nsTextControlFrame::SetValueChanged(bool aValueChanged)
{
  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element");

  if (mUsePlaceholder) {
    AutoWeakFrame weakFrame(this);
    txtCtrl->UpdateOverlayTextVisibility(true);
    if (!weakFrame.IsAlive()) {
      return;
    }
  }

  txtCtrl->SetValueChanged(aValueChanged);
}


nsresult
nsTextControlFrame::UpdateValueDisplay(bool aNotify,
                                       bool aBeforeEditorInit,
                                       const nsAString *aValue)
{
  if (!IsSingleLineTextControl()) // textareas don't use this
    return NS_OK;

  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element");
  nsIContent* rootNode = txtCtrl->GetRootEditorNode();

  NS_PRECONDITION(rootNode, "Must have a div content\n");
  NS_PRECONDITION(!mEditorHasBeenInitialized,
                  "Do not call this after editor has been initialized");
  NS_ASSERTION(!mUsePlaceholder || txtCtrl->GetPlaceholderNode(),
               "A placeholder div must exist");

  nsIContent *textContent = rootNode->GetChildAt(0);
  if (!textContent) {
    // Set up a textnode with our value
    RefPtr<nsTextNode> textNode =
      new nsTextNode(mContent->NodeInfo()->NodeInfoManager());

    NS_ASSERTION(textNode, "Must have textcontent!\n");

    rootNode->AppendChildTo(textNode, aNotify);
    textContent = textNode;
  }

  NS_ENSURE_TRUE(textContent, NS_ERROR_UNEXPECTED);

  // Get the current value of the textfield from the content.
  nsAutoString value;
  if (aValue) {
    value = *aValue;
  } else {
    txtCtrl->GetTextEditorValue(value, true);
  }

  // Update the display of the placeholder value and preview text if needed.
  // We don't need to do this if we're about to initialize the editor, since
  // EnsureEditorInitialized takes care of this.
  if ((mUsePlaceholder || mUsePreview) && !aBeforeEditorInit)
  {
    AutoWeakFrame weakFrame(this);
    txtCtrl->UpdateOverlayTextVisibility(aNotify);
    NS_ENSURE_STATE(weakFrame.IsAlive());
  }

  if (aBeforeEditorInit && value.IsEmpty()) {
    rootNode->RemoveChildAt(0, true);
    return NS_OK;
  }

  if (!value.IsEmpty() && IsPasswordTextControl()) {
    TextEditRules::FillBufWithPWChars(&value, value.Length());
  }
  return textContent->SetText(value, aNotify);
}

NS_IMETHODIMP
nsTextControlFrame::GetOwnedSelectionController(nsISelectionController** aSelCon)
{
  NS_ENSURE_ARG_POINTER(aSelCon);

  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element");

  *aSelCon = txtCtrl->GetSelectionController();
  NS_IF_ADDREF(*aSelCon);

  return NS_OK;
}

nsFrameSelection*
nsTextControlFrame::GetOwnedFrameSelection()
{
  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element");

  return txtCtrl->GetConstFrameSelection();
}

NS_IMETHODIMP
nsTextControlFrame::SaveState(nsPresState** aState)
{
  NS_ENSURE_ARG_POINTER(aState);

  *aState = nullptr;

  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element");

  nsIContent* rootNode = txtCtrl->GetRootEditorNode();
  if (rootNode) {
    // Query the nsIStatefulFrame from the HTMLScrollFrame
    nsIStatefulFrame* scrollStateFrame = do_QueryFrame(rootNode->GetPrimaryFrame());
    if (scrollStateFrame) {
      return scrollStateFrame->SaveState(aState);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTextControlFrame::RestoreState(nsPresState* aState)
{
  NS_ENSURE_ARG_POINTER(aState);

  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element");

  nsIContent* rootNode = txtCtrl->GetRootEditorNode();
  if (rootNode) {
    // Query the nsIStatefulFrame from the HTMLScrollFrame
    nsIStatefulFrame* scrollStateFrame = do_QueryFrame(rootNode->GetPrimaryFrame());
    if (scrollStateFrame) {
      return scrollStateFrame->RestoreState(aState);
    }
  }

  // Most likely, we don't have our anonymous content constructed yet, which
  // would cause us to end up here.  In this case, we'll just store the scroll
  // pos ourselves, and forward it to the scroll frame later when it's created.
  SetProperty(ContentScrollPos(), new nsPoint(aState->GetScrollPosition()));
  return NS_OK;
}

nsresult
nsTextControlFrame::PeekOffset(nsPeekOffsetStruct *aPos)
{
  return NS_ERROR_FAILURE;
}

void
nsTextControlFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                     const nsRect&           aDirtyRect,
                                     const nsDisplayListSet& aLists)
{
  /*
   * The implementation of this method is equivalent as:
   * nsContainerFrame::BuildDisplayList()
   * with the difference that we filter-out the placeholder frame when it
   * should not be visible.
   */
  DO_GLOBAL_REFLOW_COUNT_DSP("nsTextControlFrame");

  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element!");

  DisplayBorderBackgroundOutline(aBuilder, aLists);

  nsIFrame* kid = mFrames.FirstChild();
  // Redirect all lists to the Content list so that nothing can escape, ie
  // opacity creating stacking contexts that then get sorted with stacking
  // contexts external to us.
  nsDisplayList* content = aLists.Content();
  nsDisplayListSet set(content, content, content, content, content, content);

  while (kid) {
    // If the frame is the placeholder or preview frame, we should only show
    // it if it has to be visible.
    if (!((kid->GetContent() == txtCtrl->GetPlaceholderNode() &&
           !txtCtrl->GetPlaceholderVisibility()) ||
          (kid->GetContent() == txtCtrl->GetPreviewNode() &&
           !txtCtrl->GetPreviewVisibility()))) {
      BuildDisplayListForChild(aBuilder, kid, aDirtyRect, set, 0);
    }
    kid = kid->GetNextSibling();
  }
}

mozilla::dom::Element*
nsTextControlFrame::GetPseudoElement(CSSPseudoElementType aType)
{
  if (aType == CSSPseudoElementType::placeholder) {
    nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
    return txtCtrl->GetPlaceholderNode();
  }

  return nsContainerFrame::GetPseudoElement(aType);
}

NS_IMETHODIMP
nsTextControlFrame::EditorInitializer::Run()
{
  if (!mFrame) {
    return NS_OK;
  }

  // Need to block script to avoid bug 669767.
  nsAutoScriptBlocker scriptBlocker;

  nsCOMPtr<nsIPresShell> shell =
    mFrame->PresContext()->GetPresShell();
  bool observes = shell->ObservesNativeAnonMutationsForPrint();
  shell->ObserveNativeAnonMutationsForPrint(true);
  // This can cause the frame to be destroyed (and call Revoke()).
  mFrame->EnsureEditorInitialized();
  shell->ObserveNativeAnonMutationsForPrint(observes);

  // The frame can *still* be destroyed even though we have a scriptblocker,
  // bug 682684.
  if (!mFrame) {
    return NS_ERROR_FAILURE;
  }

  mFrame->FinishedInitializer();
  return NS_OK;
}
