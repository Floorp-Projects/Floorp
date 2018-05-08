/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "nsNameSpaceManager.h"
#include "nsCheckboxRadioFrame.h" //for registering accesskeys

#include "nsIContent.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsLayoutUtils.h"
#include "nsIPresShell.h"

#include <algorithm>
#include "nsRange.h" //for selection setting helper func
#include "nsINode.h"
#include "nsPIDOMWindow.h" //needed for notify selection changed to update the menus ect.
#include "nsQueryObject.h"
#include "nsILayoutHistoryState.h"

#include "nsFocusManager.h"
#include "mozilla/PresState.h"
#include "nsAttrValueInlines.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/TextEditRules.h"
#include "nsContentUtils.h"
#include "nsTextNode.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/HTMLTextAreaElement.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/Text.h"
#include "mozilla/MathAlgorithms.h"
#include "nsFrameSelection.h"

#define DEFAULT_COLUMN_WIDTH 20

using namespace mozilla;
using namespace mozilla::dom;

nsIFrame*
NS_NewTextControlFrame(nsIPresShell* aPresShell, ComputedStyle* aStyle)
{
  return new (aPresShell) nsTextControlFrame(aStyle);
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

class nsTextControlFrame::nsAnonDivObserver final : public nsStubMutationObserver
{
public:
  explicit nsAnonDivObserver(nsTextControlFrame& aFrame)
   : mFrame(aFrame) {}
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

private:
  ~nsAnonDivObserver() {}
  nsTextControlFrame& mFrame;
};

nsTextControlFrame::nsTextControlFrame(ComputedStyle* aStyle)
  : nsContainerFrame(aStyle, kClassID)
  , mFirstBaseline(NS_INTRINSIC_WIDTH_UNKNOWN)
  , mEditorHasBeenInitialized(false)
  , mIsProcessing(false)
#ifdef DEBUG
  , mInEditorInitialization(false)
#endif
{
  ClearCachedValue();
}

nsTextControlFrame::~nsTextControlFrame()
{
}

void
nsTextControlFrame::DestroyFrom(nsIFrame* aDestructRoot, PostDestroyData& aPostDestroyData)
{
  mScrollEvent.Revoke();

  DeleteProperty(TextControlInitializer());

  // Unbind the text editor state object from the frame.  The editor will live
  // on, but things like controllers will be released.
  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element");
  txtCtrl->UnbindFromFrame(this);

  nsCheckboxRadioFrame::RegUnRegAccessKey(static_cast<nsIFrame*>(this), false);

  if (mMutationObserver) {
    mRootNode->RemoveMutationObserver(mMutationObserver);
    mMutationObserver = nullptr;
  }

  // FIXME(emilio, bug 1400618): Do this after the child frames are destroyed.
  aPostDestroyData.AddAnonymousContent(mRootNode.forget());
  aPostDestroyData.AddAnonymousContent(mPlaceholderDiv.forget());
  aPostDestroyData.AddAnonymousContent(mPreviewDiv.forget());

  nsContainerFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
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
    ReflowInput::CalcLineHeight(GetContent(),
                                Style(),
                                PresContext(),
                                NS_AUTOHEIGHT,
                                aFontSizeInflation);
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

static already_AddRefed<Element>
CreateEmptyDiv(const nsTextControlFrame& aOwnerFrame)
{
  nsIDocument* doc = aOwnerFrame.PresContext()->Document();
  RefPtr<mozilla::dom::NodeInfo> nodeInfo =
    doc->NodeInfoManager()->GetNodeInfo(nsGkAtoms::div, nullptr,
                                        kNameSpaceID_XHTML,
                                        nsINode::ELEMENT_NODE);

  RefPtr<Element> element = NS_NewHTMLDivElement(nodeInfo.forget());
  return element.forget();
}

static already_AddRefed<Element>
CreateEmptyDivWithTextNode(const nsTextControlFrame& aOwnerFrame)
{
  RefPtr<Element> element = CreateEmptyDiv(aOwnerFrame);

  // Create the text node for DIV
  RefPtr<nsTextNode> textNode =
    new nsTextNode(element->OwnerDoc()->NodeInfoManager());
  textNode->MarkAsMaybeModifiedFrequently();

  element->AppendChildTo(textNode, false);

  return element.forget();
}

nsresult
nsTextControlFrame::CreateAnonymousContent(nsTArray<ContentInfo>& aElements)
{
  MOZ_ASSERT(mContent, "We should have a content!");

  AddStateBits(NS_FRAME_INDEPENDENT_SELECTION);

  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  MOZ_ASSERT(txtCtrl, "Content not a text control element");

  nsresult rv = CreateRootNode();
  NS_ENSURE_SUCCESS(rv, rv);

  // Bind the frame to its text control
  rv = txtCtrl->BindToFrame(this);
  NS_ENSURE_SUCCESS(rv, rv);

  aElements.AppendElement(mRootNode);
  CreatePlaceholderIfNeeded();
  if (mPlaceholderDiv) {
    if (!IsSingleLineTextControl()) {
      // For textareas, UpdateValueDisplay doesn't initialize the visibility
      // status of the placeholder because it returns early, so we have to
      // do that manually here.
      txtCtrl->UpdateOverlayTextVisibility(true);
    }
    aElements.AppendElement(mPlaceholderDiv);
  }
  CreatePreviewIfNeeded();
  if (mPreviewDiv) {
    aElements.AppendElement(mPreviewDiv);
  }

  rv = UpdateValueDisplay(false);
  NS_ENSURE_SUCCESS(rv, rv);

  InitializeEagerlyIfNeeded();
  return NS_OK;
}

bool
nsTextControlFrame::ShouldInitializeEagerly() const
{
  // textareas are eagerly initialized.
  if (!IsSingleLineTextControl()) {
    return true;
  }

  // Also, input elements which have a cached selection should get eager
  // editor initialization.
  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  if (txtCtrl->HasCachedSelection()) {
    return true;
  }

  // So do input text controls with spellcheck=true
  if (auto* htmlElement = nsGenericHTMLElement::FromNode(mContent)) {
    if (htmlElement->Spellcheck()) {
      return true;
    }
  }

  return false;
}

void
nsTextControlFrame::InitializeEagerlyIfNeeded()
{
  MOZ_ASSERT(!nsContentUtils::IsSafeToRunScript(),
             "Someone forgot a script blocker?");
  if (!ShouldInitializeEagerly()) {
    return;
  }

  EditorInitializer* initializer = new EditorInitializer(this);
  SetProperty(TextControlInitializer(), initializer);
  nsContentUtils::AddScriptRunner(initializer);
}

nsresult
nsTextControlFrame::CreateRootNode()
{
  MOZ_ASSERT(!mRootNode);

  mRootNode = CreateEmptyDiv(*this);

  mMutationObserver = new nsAnonDivObserver(*this);
  mRootNode->AddMutationObserver(mMutationObserver);

  // Make our root node editable
  mRootNode->SetFlags(NODE_IS_EDITABLE);

  // Set the necessary classes on the text control. We use class values instead
  // of a 'style' attribute so that the style comes from a user-agent style
  // sheet and is still applied even if author styles are disabled.
  nsAutoString classValue;
  classValue.AppendLiteral("anonymous-div");
  if (GetWrapCols() > 0) {
    classValue.AppendLiteral(" wrap");
  }

  if (!IsSingleLineTextControl()) {
    // We can't just inherit the overflow because setting visible overflow will
    // crash when the number of lines exceeds the height of the textarea and
    // setting -moz-hidden-unscrollable overflow (NS_STYLE_OVERFLOW_CLIP)
    // doesn't paint the caret for some reason.
    const nsStyleDisplay* disp = StyleDisplay();
    if (disp->mOverflowX != NS_STYLE_OVERFLOW_VISIBLE &&
        disp->mOverflowX != NS_STYLE_OVERFLOW_CLIP) {
      classValue.AppendLiteral(" inherit-overflow");
    }
    classValue.AppendLiteral(" inherit-scroll-behavior");
  }
  nsresult rv = mRootNode->SetAttr(kNameSpaceID_None, nsGkAtoms::_class,
                                   classValue, false);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
nsTextControlFrame::CreatePlaceholderIfNeeded()
{
  MOZ_ASSERT(!mPlaceholderDiv);

  // Do we need a placeholder node?
  nsAutoString placeholderTxt;
  mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::placeholder,
                                 placeholderTxt);
  if (IsTextArea()) { // <textarea>s preserve newlines...
    nsContentUtils::PlatformToDOMLineBreaks(placeholderTxt);
  } else { // ...<input>s don't
    nsContentUtils::RemoveNewlines(placeholderTxt);
  }

  if (placeholderTxt.IsEmpty()) {
    return;
  }

  mPlaceholderDiv = CreateEmptyDivWithTextNode(*this);
  // Associate ::placeholder pseudo-element with the placeholder node.
  mPlaceholderDiv->SetPseudoElementType(CSSPseudoElementType::placeholder);
  mPlaceholderDiv->GetFirstChild()->AsText()->SetText(placeholderTxt, false);
}

void
nsTextControlFrame::CreatePreviewIfNeeded()
{
  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  if (!txtCtrl->IsPreviewEnabled()) {
    return;
  }

  mPreviewDiv = CreateEmptyDivWithTextNode(*this);
  mPreviewDiv->SetAttr(kNameSpaceID_None, nsGkAtoms::_class,
                       NS_LITERAL_STRING("preview-div"), false);
}

void
nsTextControlFrame::AppendAnonymousContentTo(
  nsTArray<nsIContent*>& aElements,
  uint32_t aFilter)
{
  aElements.AppendElement(mRootNode);

  if (mPlaceholderDiv && !(aFilter & nsIContent::eSkipPlaceholderContent)) {
    aElements.AppendElement(mPlaceholderDiv);
  }

  if (mPreviewDiv) {
    aElements.AppendElement(mPreviewDiv);
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
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  // make sure that the form registers itself on the initial/first reflow
  if (mState & NS_FRAME_FIRST_REFLOW) {
    nsCheckboxRadioFrame::RegUnRegAccessKey(this, true);
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
    lineHeight = ReflowInput::CalcLineHeight(GetContent(),
                                             Style(),
                                             PresContext(),
                                             NS_AUTOHEIGHT,
                                             inflation);
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

  aStatus.Reset(); // This type of frame can't be split.
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
  if (mPlaceholderDiv) {
    txtCtrl->UpdateOverlayTextVisibility(true);
  }

  if (!aOn) {
    return;
  }

  nsISelectionController* selCon = txtCtrl->GetSelectionController();
  if (!selCon) {
    return;
  }

  RefPtr<Selection> ourSel =
    selCon->GetSelection(nsISelectionController::SELECTION_NORMAL);
  if (!ourSel) {
    return;
  }

  nsIPresShell* presShell = PresContext()->GetPresShell();
  RefPtr<nsCaret> caret = presShell->GetCaret();
  if (!caret) {
    return;
  }

  // Scroll the current selection into view
  Selection* caretSelection = caret->GetSelection();
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
      nsresult rv = mContent->OwnerDoc()->Dispatch(TaskCategory::Other,
                                                   do_AddRef(event));
      if (NS_SUCCEEDED(rv)) {
        mScrollEvent = Move(event);
      }
    }
  }

  // tell the caret to use our selection
  caret->SetSelection(ourSel);

  // mutual-exclusion: the selection is either controlled by the
  // document or by the text input/area. Clear any selection in the
  // document since the focus is now on our independent selection.

  nsCOMPtr<nsISelectionController> selcon = do_QueryInterface(presShell);
  RefPtr<Selection> docSel =
    selcon->GetSelection(nsISelectionController::SELECTION_NORMAL);
  if (!docSel) {
    return;
  }

  if (!docSel->IsCollapsed()) {
    docSel->RemoveAllRanges(IgnoreErrors());
  }
}

nsresult nsTextControlFrame::SetFormProperty(nsAtom* aName, const nsAString& aValue)
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

NS_IMETHODIMP_(already_AddRefed<TextEditor>)
nsTextControlFrame::GetTextEditor()
{
  if (NS_WARN_IF(NS_FAILED(EnsureEditorInitialized()))) {
    return nullptr;
  }

  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  MOZ_ASSERT(txtCtrl, "Content not a text control element");
  RefPtr<TextEditor> textEditor = txtCtrl->GetTextEditor();
  return textEditor.forget();
}

nsresult
nsTextControlFrame::SetSelectionInternal(nsINode* aStartNode,
                                         uint32_t aStartOffset,
                                         nsINode* aEndNode,
                                         uint32_t aEndOffset,
                                         nsITextControlFrame::SelectionDirection aDirection)
{
  // Create a new range to represent the new selection.
  // Note that we use a new range to avoid having to do
  // isIncreasing checks to avoid possible errors.

  RefPtr<nsRange> range = new nsRange(mContent);
  // Be careful to use internal nsRange methods which do not check to make sure
  // we have access to the node.
  // XXXbz nsRange::SetStartAndEnd takes int32_t (and ranges generally work on
  // int32_t), but we're passing uint32_t.  The good news is that at this point
  // our endpoints should really be within our length, so not really that big.
  // And if they _are_ that big, SetStartAndEnd() will simply error out, which
  // is not too bad for a case we don't expect to happen.
  nsresult rv = range->SetStartAndEnd(aStartNode, aStartOffset,
				      aEndNode, aEndOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the selection, clear it and add the new range to it!
  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element");
  nsISelectionController* selCon = txtCtrl->GetSelectionController();
  NS_ENSURE_TRUE(selCon, NS_ERROR_FAILURE);

  RefPtr<Selection> selection =
    selCon->GetSelection(nsISelectionController::SELECTION_NORMAL);
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);

  nsDirection direction;
  if (aDirection == eNone) {
    // Preserve the direction
    direction = selection->GetDirection();
  } else {
    direction = (aDirection == eBackward) ? eDirPrevious : eDirNext;
  }

  ErrorResult err;
  selection->RemoveAllRanges(err);
  if (NS_WARN_IF(err.Failed())) {
    return err.StealNSResult();
  }

  selection->AddRange(*range, err);  // NOTE: can destroy the world
  if (NS_WARN_IF(err.Failed())) {
    return err.StealNSResult();
  }

  selection->SetDirection(direction);
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
nsTextControlFrame::SelectAllOrCollapseToEndOfText(bool aSelect)
{
  nsresult rv = EnsureEditorInitialized();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsINode> rootNode;
  rootNode= mRootNode;

  NS_ENSURE_TRUE(rootNode, NS_ERROR_FAILURE);

  int32_t numChildren = mRootNode->GetChildCount();

  if (numChildren > 0) {
    // We never want to place the selection after the last
    // br under the root node!
    nsIContent *child = mRootNode->GetLastChild();
    if (child) {
      if (child->IsHTMLElement(nsGkAtoms::br)) {
        child = child->GetPreviousSibling();
        --numChildren;
      } else if (child->IsText() && !child->Length()) {
        // Editor won't remove text node when empty value.
        --numChildren;
      }
    }
    if (!aSelect && numChildren) {
      child = child->GetPreviousSibling();
      if (child && child->IsText()) {
        rootNode = child;
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

  nsCOMPtr<nsINode> startNode, endNode;
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
                                     nsINode** aResult,
                                     uint32_t* aPosition)
{
  NS_ENSURE_ARG_POINTER(aResult && aPosition);

  *aResult = nullptr;
  *aPosition = 0;

  nsresult rv = EnsureEditorInitialized();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RefPtr<Element> rootNode = mRootNode;
  NS_ENSURE_TRUE(rootNode, NS_ERROR_FAILURE);

  nsCOMPtr<nsINodeList> nodeList = rootNode->ChildNodes();
  uint32_t length = nodeList->Length();

  NS_ASSERTION(length <= 2, "We should have one text node and one mozBR at most");

  nsCOMPtr<nsINode> firstNode = nodeList->Item(0);
  Text* textNode = firstNode ? firstNode->GetAsText() : nullptr;

  if (length == 0) {
    rootNode.forget(aResult);
    *aPosition = 0;
  } else if (textNode) {
    uint32_t textLength = textNode->Length();
    if (length == 2 && aOffset == textLength) {
      // If we're at the end of the text node and we have a trailing BR node,
      // set the selection on the BR node.
      rootNode.forget(aResult);
      *aPosition = 1;
    } else {
      // Otherwise, set the selection on the textnode itself.
      firstNode.forget(aResult);
      *aPosition = std::min(aOffset, textLength);
    }
  } else {
    rootNode.forget(aResult);
    *aPosition = 0;
  }

  return NS_OK;
}

/////END INTERFACE IMPLEMENTATIONS

////NSIFRAME
nsresult
nsTextControlFrame::AttributeChanged(int32_t         aNameSpaceID,
                                     nsAtom*        aAttribute,
                                     int32_t         aModType)
{
  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element");
  nsISelectionController* selCon = txtCtrl->GetSelectionController();
  const bool needEditor = nsGkAtoms::maxlength == aAttribute ||
                            nsGkAtoms::readonly == aAttribute ||
                            nsGkAtoms::disabled == aAttribute ||
                            nsGkAtoms::spellcheck == aAttribute;
  RefPtr<TextEditor> textEditor = needEditor ? GetTextEditor() : nullptr;
  if ((needEditor && !textEditor) || !selCon) {
    return nsContainerFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
  }

  if (nsGkAtoms::maxlength == aAttribute) {
    int32_t maxLength;
    bool maxDefined = GetMaxLength(&maxLength);
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
    if (AttributeExists(nsGkAtoms::readonly)) { // set readonly
      if (nsContentUtils::IsFocusedContent(mContent)) {
        selCon->SetCaretEnabled(false);
      }
      textEditor->AddFlags(nsIPlaintextEditor::eEditorReadonlyMask);
    } else { // unset readonly
      if (!textEditor->IsDisabled() &&
          nsContentUtils::IsFocusedContent(mContent)) {
        selCon->SetCaretEnabled(true);
      }
      textEditor->RemoveFlags(nsIPlaintextEditor::eEditorReadonlyMask);
    }
    return NS_OK;
  }

  if (nsGkAtoms::disabled == aAttribute) {
    int16_t displaySelection = nsISelectionController::SELECTION_OFF;
    const bool focused = nsContentUtils::IsFocusedContent(mContent);
    const bool hasAttr = AttributeExists(nsGkAtoms::disabled);
    bool disable;
    if (hasAttr) { // set disabled
      disable = true;
    } else { // unset disabled
      disable = false;
      displaySelection = focused ? nsISelectionController::SELECTION_ON
                                 : nsISelectionController::SELECTION_HIDDEN;
    }
    selCon->SetDisplaySelection(displaySelection);
    if (focused) {
      selCon->SetCaretEnabled(!hasAttr);
    }
    if (disable) {
      textEditor->AddFlags(nsIPlaintextEditor::eEditorDisabledMask);
    } else {
      textEditor->RemoveFlags(nsIPlaintextEditor::eEditorDisabledMask);
    }
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


void
nsTextControlFrame::GetText(nsString& aText)
{
  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element");
  if (IsSingleLineTextControl()) {
    // There will be no line breaks so we can ignore the wrap property.
    txtCtrl->GetTextEditorValue(aText, true);
  } else {
    HTMLTextAreaElement* textArea = HTMLTextAreaElement::FromNode(mContent);
    if (textArea) {
      textArea->GetValue(aText);
    }
  }
}


///END NSIFRAME OVERLOADS
/////BEGIN PROTECTED METHODS

bool
nsTextControlFrame::GetMaxLength(int32_t* aSize)
{
  *aSize = -1;

  nsGenericHTMLElement *content = nsGenericHTMLElement::FromNode(mContent);
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
      UniquePtr<PresState> fakePresState = NewPresState();
      fakePresState->scrollState() = *contentScrollPos;
      statefulFrame->RestoreState(fakePresState.get());
      RemoveProperty(ContentScrollPos());
      delete contentScrollPos;
    }
  }
}

void
nsTextControlFrame::SetValueChanged(bool aValueChanged)
{
  nsCOMPtr<nsITextControlElement> txtCtrl = HTMLInputElement::FromNode(GetContent());
  if (!txtCtrl) {
    txtCtrl = HTMLTextAreaElement::FromNode(GetContent());
  }
  MOZ_ASSERT(txtCtrl, "Content not a text control element");

  if (mPlaceholderDiv) {
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

  MOZ_ASSERT(mRootNode, "Must have a div content\n");
  MOZ_ASSERT(!mEditorHasBeenInitialized,
             "Do not call this after editor has been initialized");

  nsIContent* childContent = mRootNode->GetFirstChild();
  Text* textContent;
  if (!childContent) {
    // Set up a textnode with our value
    RefPtr<nsTextNode> textNode =
      new nsTextNode(mContent->NodeInfo()->NodeInfoManager());
    textNode->MarkAsMaybeModifiedFrequently();

    mRootNode->AppendChildTo(textNode, aNotify);
    textContent = textNode;
  } else {
    textContent = childContent->AsText();
  }

  NS_ENSURE_TRUE(textContent, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  MOZ_ASSERT(txtCtrl);

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
  if ((mPlaceholderDiv || mPreviewDiv) && !aBeforeEditorInit) {
    AutoWeakFrame weakFrame(this);
    txtCtrl->UpdateOverlayTextVisibility(aNotify);
    NS_ENSURE_STATE(weakFrame.IsAlive());
  }

  if (aBeforeEditorInit && value.IsEmpty()) {
    nsIContent* node = mRootNode->GetFirstChild();
    if (node) {
      mRootNode->RemoveChildNode(node, true);
    }
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

UniquePtr<PresState>
nsTextControlFrame::SaveState()
{
  if (mRootNode) {
    // Query the nsIStatefulFrame from the HTMLScrollFrame
    nsIStatefulFrame* scrollStateFrame =
      do_QueryFrame(mRootNode->GetPrimaryFrame());
    if (scrollStateFrame) {
      return scrollStateFrame->SaveState();
    }
  }

  return nullptr;
}

NS_IMETHODIMP
nsTextControlFrame::RestoreState(PresState* aState)
{
  NS_ENSURE_ARG_POINTER(aState);

  if (mRootNode) {
    // Query the nsIStatefulFrame from the HTMLScrollFrame
    nsIStatefulFrame* scrollStateFrame =
      do_QueryFrame(mRootNode->GetPrimaryFrame());
    if (scrollStateFrame) {
      return scrollStateFrame->RestoreState(aState);
    }
  }

  // Most likely, we don't have our anonymous content constructed yet, which
  // would cause us to end up here.  In this case, we'll just store the scroll
  // pos ourselves, and forward it to the scroll frame later when it's created.
  SetProperty(ContentScrollPos(), new nsPoint(aState->scrollState()));
  return NS_OK;
}

nsresult
nsTextControlFrame::PeekOffset(nsPeekOffsetStruct *aPos)
{
  return NS_ERROR_FAILURE;
}

void
nsTextControlFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
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
    if (!((kid->GetContent() == mPlaceholderDiv &&
           !txtCtrl->GetPlaceholderVisibility()) ||
          (kid->GetContent() == mPreviewDiv &&
           !txtCtrl->GetPreviewVisibility()))) {
      BuildDisplayListForChild(aBuilder, kid, set, 0);
    }
    kid = kid->GetNextSibling();
  }
}

mozilla::dom::Element*
nsTextControlFrame::GetPseudoElement(CSSPseudoElementType aType)
{
  if (aType == CSSPseudoElementType::placeholder) {
    return mPlaceholderDiv;
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

NS_IMPL_ISUPPORTS(nsTextControlFrame::nsAnonDivObserver, nsIMutationObserver)

void
nsTextControlFrame::nsAnonDivObserver::CharacterDataChanged(
  nsIContent* aContent,
  const CharacterDataChangeInfo&)
{
  mFrame.ClearCachedValue();
}

void
nsTextControlFrame::nsAnonDivObserver::ContentAppended(
  nsIContent* aFirstNewContent)
{
  mFrame.ClearCachedValue();
}

void
nsTextControlFrame::nsAnonDivObserver::ContentInserted(nsIContent* aChild)
{
  mFrame.ClearCachedValue();
}

void
nsTextControlFrame::nsAnonDivObserver::ContentRemoved(
  nsIContent* aChild,
  nsIContent* aPreviousSibling)
{
  mFrame.ClearCachedValue();
}
