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
#include "nsIEditor.h"
#include "nsCaret.h"
#include "nsCSSPseudoElements.h"
#include "nsGenericHTMLElement.h"
#include "nsTextFragment.h"
#include "nsNameSpaceManager.h"
#include "nsCheckboxRadioFrame.h"  //for registering accesskeys

#include "nsIContent.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsLayoutUtils.h"

#include <algorithm>
#include "nsRange.h"  //for selection setting helper func
#include "nsINode.h"
#include "nsPIDOMWindow.h"  //needed for notify selection changed to update the menus ect.
#include "nsQueryObject.h"
#include "nsILayoutHistoryState.h"

#include "nsFocusManager.h"
#include "mozilla/PresShell.h"
#include "mozilla/PresState.h"
#include "nsAttrValueInlines.h"
#include "mozilla/dom/Selection.h"
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

nsIFrame* NS_NewTextControlFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell)
      nsTextControlFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsTextControlFrame)

NS_QUERYFRAME_HEAD(nsTextControlFrame)
  NS_QUERYFRAME_ENTRY(nsIFormControlFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
  NS_QUERYFRAME_ENTRY(nsITextControlFrame)
  NS_QUERYFRAME_ENTRY(nsIStatefulFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

#ifdef ACCESSIBILITY
a11y::AccType nsTextControlFrame::AccessibleType() {
  return a11y::eHTMLTextFieldType;
}
#endif

#ifdef DEBUG
class EditorInitializerEntryTracker {
 public:
  explicit EditorInitializerEntryTracker(nsTextControlFrame& frame)
      : mFrame(frame), mFirstEntry(false) {
    if (!mFrame.mInEditorInitialization) {
      mFrame.mInEditorInitialization = true;
      mFirstEntry = true;
    }
  }
  ~EditorInitializerEntryTracker() {
    if (mFirstEntry) {
      mFrame.mInEditorInitialization = false;
    }
  }
  bool EnteredMoreThanOnce() const { return !mFirstEntry; }

 private:
  nsTextControlFrame& mFrame;
  bool mFirstEntry;
};
#endif

class nsTextControlFrame::nsAnonDivObserver final
    : public nsStubMutationObserver {
 public:
  explicit nsAnonDivObserver(nsTextControlFrame& aFrame) : mFrame(aFrame) {}
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

 private:
  ~nsAnonDivObserver() = default;
  nsTextControlFrame& mFrame;
};

nsTextControlFrame::nsTextControlFrame(ComputedStyle* aStyle,
                                       nsPresContext* aPresContext,
                                       nsIFrame::ClassID aClassID)
    : nsContainerFrame(aStyle, aPresContext, aClassID),
      mFirstBaseline(NS_INTRINSIC_ISIZE_UNKNOWN),
      mEditorHasBeenInitialized(false),
      mIsProcessing(false)
#ifdef DEBUG
      ,
      mInEditorInitialization(false)
#endif
{
  ClearCachedValue();
}

nsTextControlFrame::~nsTextControlFrame() = default;

nsIScrollableFrame* nsTextControlFrame::GetScrollTargetFrame() {
  if (!mRootNode) {
    return nullptr;
  }
  return do_QueryFrame(mRootNode->GetPrimaryFrame());
}

void nsTextControlFrame::DestroyFrom(nsIFrame* aDestructRoot,
                                     PostDestroyData& aPostDestroyData) {
  mScrollEvent.Revoke();

  RemoveProperty(TextControlInitializer());

  // Unbind the text editor state object from the frame.  The editor will live
  // on, but things like controllers will be released.
  RefPtr<TextControlElement> textControlElement =
      TextControlElement::FromNode(GetContent());
  MOZ_ASSERT(textControlElement);
  textControlElement->UnbindFromFrame(this);

  nsCheckboxRadioFrame::RegUnRegAccessKey(static_cast<nsIFrame*>(this), false);

  if (mMutationObserver) {
    mRootNode->RemoveMutationObserver(mMutationObserver);
    mMutationObserver = nullptr;
  }

  // If we're a subclass like nsNumberControlFrame, then it owns the root of the
  // anonymous subtree where mRootNode is.
  if (mClass == kClassID) {
    aPostDestroyData.AddAnonymousContent(mRootNode.forget());
  } else {
    MOZ_ASSERT(!mRootNode || !mRootNode->IsRootOfAnonymousSubtree());
    mRootNode = nullptr;
  }

  aPostDestroyData.AddAnonymousContent(mPlaceholderDiv.forget());
  aPostDestroyData.AddAnonymousContent(mPreviewDiv.forget());

  nsContainerFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

LogicalSize nsTextControlFrame::CalcIntrinsicSize(
    gfxContext* aRenderingContext, WritingMode aWM,
    float aFontSizeInflation) const {
  LogicalSize intrinsicSize(aWM);
  // Get leading and the Average/MaxAdvance char width
  nscoord lineHeight = 0;
  nscoord charWidth = 0;
  nscoord charMaxAdvance = 0;

  RefPtr<nsFontMetrics> fontMet =
      nsLayoutUtils::GetFontMetricsForFrame(this, aFontSizeInflation);

  lineHeight =
      ReflowInput::CalcLineHeight(GetContent(), Style(), PresContext(),
                                  NS_UNCONSTRAINEDSIZE, aFontSizeInflation);
  charWidth = fontMet->AveCharWidth();
  charMaxAdvance = fontMet->MaxAdvance();

  // Set the width equal to the width in characters
  int32_t cols = GetCols();
  intrinsicSize.ISize(aWM) = cols * charWidth;

  // To better match IE, take the maximum character width(in twips) and remove
  // 4 pixels add this on as additional padding(internalPadding). But only do
  // this if we think we have a fixed-width font.
  if (mozilla::Abs(charWidth - charMaxAdvance) >
      (unsigned)nsPresContext::CSSPixelsToAppUnits(1)) {
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
    const StyleLength& letterSpacing = StyleText()->mLetterSpacing;
    if (!letterSpacing.IsZero()) {
      intrinsicSize.ISize(aWM) += cols * letterSpacing.ToAppUnits();
    }
  }

  // Set the height equal to total number of rows (times the height of each
  // line, of course)
  intrinsicSize.BSize(aWM) = lineHeight * GetRows();

  // Add in the size of the scrollbars for textarea
  if (IsTextArea()) {
    nsIScrollableFrame* scrollableFrame = GetScrollTargetFrame();
    NS_ASSERTION(scrollableFrame, "Child must be scrollable");

    if (scrollableFrame) {
      LogicalMargin scrollbarSizes(
          aWM, scrollableFrame->GetDesiredScrollbarSizes(PresContext(),
                                                         aRenderingContext));

      intrinsicSize.ISize(aWM) += scrollbarSizes.IStartEnd(aWM);
      intrinsicSize.BSize(aWM) += scrollbarSizes.BStartEnd(aWM);
    }
  }
  return intrinsicSize;
}

nsresult nsTextControlFrame::EnsureEditorInitialized() {
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

  if (mEditorHasBeenInitialized) return NS_OK;

  Document* doc = mContent->GetComposedDoc();
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
    RefPtr<TextControlElement> textControlElement =
        TextControlElement::FromNode(GetContent());
    MOZ_ASSERT(textControlElement);

    // Hide selection changes during the initialization, as webpages should not
    // be aware of these initializations
    AutoHideSelectionChanges hideSelectionChanges(
        textControlElement->GetConstFrameSelection());

    nsAutoScriptBlocker scriptBlocker;

    // Time to mess with our security context... See comments in GetValue()
    // for why this is needed.
    mozilla::dom::AutoNoJSAPI nojsapi;

    // Make sure that we try to focus the content even if the method fails
    class EnsureSetFocus {
     public:
      explicit EnsureSetFocus(nsTextControlFrame* aFrame) : mFrame(aFrame) {}
      ~EnsureSetFocus() {
        if (nsContentUtils::IsFocusedContent(mFrame->GetContent()))
          mFrame->SetFocus(true, false);
      }

     private:
      nsTextControlFrame* mFrame;
    };
    EnsureSetFocus makeSureSetFocusHappens(this);

#ifdef DEBUG
    // Make sure we are not being called again until we're finished.
    // If reentrancy happens, just pretend that we don't have an editor.
    const EditorInitializerEntryTracker tracker(*this);
    NS_ASSERTION(!tracker.EnteredMoreThanOnce(),
                 "EnsureEditorInitialized has been called while a previous "
                 "call was in progress");
#endif

    // Create an editor for the frame, if one doesn't already exist
    nsresult rv = textControlElement->CreateEditor();
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_STATE(weakFrame.IsAlive());

    // Set mEditorHasBeenInitialized so that subsequent calls will use the
    // editor.
    mEditorHasBeenInitialized = true;

    if (weakFrame.IsAlive()) {
      uint32_t position = 0;

      // Set the selection to the end of the text field (bug 1287655),
      // but only if the contents has changed (bug 1337392).
      if (textControlElement->ValueChanged()) {
        nsAutoString val;
        textControlElement->GetTextEditorValue(val, true);
        position = val.Length();
      }

      SetSelectionEndPoints(position, position);
    }
  }
  NS_ENSURE_STATE(weakFrame.IsAlive());
  return NS_OK;
}

already_AddRefed<Element> nsTextControlFrame::CreateEmptyAnonymousDiv(
    PseudoStyleType aPseudoType) const {
  MOZ_ASSERT(aPseudoType != PseudoStyleType::NotPseudo);
  Document* doc = PresContext()->Document();
  RefPtr<NodeInfo> nodeInfo = doc->NodeInfoManager()->GetNodeInfo(
      nsGkAtoms::div, nullptr, kNameSpaceID_XHTML, nsINode::ELEMENT_NODE);
  RefPtr<Element> div = NS_NewHTMLDivElement(nodeInfo.forget());
  div->SetPseudoElementType(aPseudoType);
  if (aPseudoType == PseudoStyleType::mozTextControlEditingRoot) {
    // Make our root node editable
    div->SetFlags(NODE_IS_EDITABLE);
  }
  return div.forget();
}

already_AddRefed<Element>
nsTextControlFrame::CreateEmptyAnonymousDivWithTextNode(
    PseudoStyleType aPseudoType) const {
  RefPtr<Element> divElement = CreateEmptyAnonymousDiv(aPseudoType);

  // Create the text node for the anonymous <div> element.
  RefPtr<nsTextNode> textNode = new (divElement->OwnerDoc()->NodeInfoManager())
      nsTextNode(divElement->OwnerDoc()->NodeInfoManager());
  // If the anonymous div element is not for the placeholder, we should
  // mark the text node as "maybe modified frequently" for avoiding ASCII
  // range checks at every input.
  if (aPseudoType != PseudoStyleType::placeholder) {
    textNode->MarkAsMaybeModifiedFrequently();
    // Additionally, this is a password field, the text node needs to be
    // marked as "maybe masked" unless it's in placeholder.
    if (IsPasswordTextControl()) {
      textNode->MarkAsMaybeMasked();
    }
  }
  divElement->AppendChildTo(textNode, false);
  return divElement.forget();
}

nsresult nsTextControlFrame::CreateAnonymousContent(
    nsTArray<ContentInfo>& aElements) {
  MOZ_ASSERT(!nsContentUtils::IsSafeToRunScript());
  MOZ_ASSERT(mContent, "We should have a content!");

  AddStateBits(NS_FRAME_INDEPENDENT_SELECTION);

  RefPtr<TextControlElement> textControlElement =
      TextControlElement::FromNode(GetContent());
  MOZ_ASSERT(textControlElement);
  mRootNode =
      CreateEmptyAnonymousDiv(PseudoStyleType::mozTextControlEditingRoot);
  if (NS_WARN_IF(!mRootNode)) {
    return NS_ERROR_FAILURE;
  }

  mMutationObserver = new nsAnonDivObserver(*this);
  mRootNode->AddMutationObserver(mMutationObserver);

  // Bind the frame to its text control.
  //
  // This can realistically fail in paginated mode, where we may replicate
  // fixed-positioned elements and the replicated frame will not get the chance
  // to get an editor.
  nsresult rv = textControlElement->BindToFrame(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mRootNode->RemoveMutationObserver(mMutationObserver);
    mMutationObserver = nullptr;
    mRootNode = nullptr;
    return rv;
  }

  aElements.AppendElement(mRootNode);
  CreatePlaceholderIfNeeded();
  if (mPlaceholderDiv) {
    if (!IsSingleLineTextControl()) {
      // For textareas, UpdateValueDisplay doesn't initialize the visibility
      // status of the placeholder because it returns early, so we have to
      // do that manually here.
      textControlElement->UpdateOverlayTextVisibility(true);
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

bool nsTextControlFrame::ShouldInitializeEagerly() const {
  // textareas are eagerly initialized.
  if (!IsSingleLineTextControl()) {
    return true;
  }

  // Also, input elements which have a cached selection should get eager
  // editor initialization.
  TextControlElement* textControlElement =
      TextControlElement::FromNode(GetContent());
  MOZ_ASSERT(textControlElement);
  if (textControlElement->HasCachedSelection()) {
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

void nsTextControlFrame::InitializeEagerlyIfNeeded() {
  MOZ_ASSERT(!nsContentUtils::IsSafeToRunScript(),
             "Someone forgot a script blocker?");
  if (!ShouldInitializeEagerly()) {
    return;
  }

  EditorInitializer* initializer = new EditorInitializer(this);
  SetProperty(TextControlInitializer(), initializer);
  nsContentUtils::AddScriptRunner(initializer);
}

void nsTextControlFrame::CreatePlaceholderIfNeeded() {
  MOZ_ASSERT(!mPlaceholderDiv);

  // Do we need a placeholder node?
  nsAutoString placeholderTxt;
  mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::placeholder,
                                 placeholderTxt);
  if (IsTextArea()) {  // <textarea>s preserve newlines...
    nsContentUtils::PlatformToDOMLineBreaks(placeholderTxt);
  } else {  // ...<input>s don't
    nsContentUtils::RemoveNewlines(placeholderTxt);
  }

  if (placeholderTxt.IsEmpty()) {
    return;
  }

  mPlaceholderDiv =
      CreateEmptyAnonymousDivWithTextNode(PseudoStyleType::placeholder);
  mPlaceholderDiv->GetFirstChild()->AsText()->SetText(placeholderTxt, false);
}

void nsTextControlFrame::CreatePreviewIfNeeded() {
  RefPtr<TextControlElement> textControlElement =
      TextControlElement::FromNode(GetContent());
  MOZ_ASSERT(textControlElement);
  if (!textControlElement->IsPreviewEnabled()) {
    return;
  }

  mPreviewDiv = CreateEmptyAnonymousDivWithTextNode(
      PseudoStyleType::mozTextControlPreview);
}

void nsTextControlFrame::AppendAnonymousContentTo(
    nsTArray<nsIContent*>& aElements, uint32_t aFilter) {
  aElements.AppendElement(mRootNode);

  if (mPlaceholderDiv && !(aFilter & nsIContent::eSkipPlaceholderContent)) {
    aElements.AppendElement(mPlaceholderDiv);
  }

  if (mPreviewDiv) {
    aElements.AppendElement(mPreviewDiv);
  }
}

nscoord nsTextControlFrame::GetPrefISize(gfxContext* aRenderingContext) {
  nscoord result = 0;
  DISPLAY_PREF_INLINE_SIZE(this, result);
  float inflation = nsLayoutUtils::FontSizeInflationFor(this);
  WritingMode wm = GetWritingMode();
  result = CalcIntrinsicSize(aRenderingContext, wm, inflation).ISize(wm);
  return result;
}

nscoord nsTextControlFrame::GetMinISize(gfxContext* aRenderingContext) {
  // Our min inline size is just our preferred width if we have auto inline size
  nscoord result;
  DISPLAY_MIN_INLINE_SIZE(this, result);
  result = GetPrefISize(aRenderingContext);
  return result;
}

LogicalSize nsTextControlFrame::ComputeAutoSize(
    gfxContext* aRenderingContext, WritingMode aWM, const LogicalSize& aCBSize,
    nscoord aAvailableISize, const LogicalSize& aMargin,
    const LogicalSize& aBorder, const LogicalSize& aPadding,
    ComputeSizeFlags aFlags) {
  float inflation = nsLayoutUtils::FontSizeInflationFor(this);
  LogicalSize autoSize = CalcIntrinsicSize(aRenderingContext, aWM, inflation);

  // Note: nsContainerFrame::ComputeAutoSize only computes the inline-size (and
  // only for 'auto'), the block-size it returns is always NS_UNCONSTRAINEDSIZE.
  const auto& iSizeCoord = StylePosition()->ISize(aWM);
  if (iSizeCoord.IsAuto()) {
    if (aFlags & ComputeSizeFlags::eIClampMarginBoxMinSize) {
      // CalcIntrinsicSize isn't aware of grid-item margin-box clamping, so we
      // fall back to nsContainerFrame's ComputeAutoSize to handle that.
      // XXX maybe a font-inflation issue here? (per the assertion below).
      autoSize.ISize(aWM) =
          nsContainerFrame::ComputeAutoSize(aRenderingContext, aWM, aCBSize,
                                            aAvailableISize, aMargin, aBorder,
                                            aPadding, aFlags)
              .ISize(aWM);
    }
#ifdef DEBUG
    else {
      LogicalSize ancestorAutoSize = nsContainerFrame::ComputeAutoSize(
          aRenderingContext, aWM, aCBSize, aAvailableISize, aMargin, aBorder,
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

void nsTextControlFrame::ComputeBaseline(const ReflowInput& aReflowInput,
                                         ReflowOutput& aDesiredSize) {
  // If we're layout-contained, we have no baseline.
  if (aReflowInput.mStyleDisplay->IsContainLayout()) {
    mFirstBaseline = NS_INTRINSIC_ISIZE_UNKNOWN;
    return;
  }
  WritingMode wm = aReflowInput.GetWritingMode();

  // Calculate the baseline and store it in mFirstBaseline.
  nscoord lineHeight = aReflowInput.ComputedBSize();
  float inflation = nsLayoutUtils::FontSizeInflationFor(this);
  if (!IsSingleLineTextControl()) {
    lineHeight = ReflowInput::CalcLineHeight(
        GetContent(), Style(), PresContext(), NS_UNCONSTRAINEDSIZE, inflation);
  }
  RefPtr<nsFontMetrics> fontMet =
      nsLayoutUtils::GetFontMetricsForFrame(this, inflation);
  mFirstBaseline = nsLayoutUtils::GetCenteredFontBaseline(fontMet, lineHeight,
                                                          wm.IsLineInverted()) +
                   aReflowInput.ComputedLogicalBorderPadding().BStart(wm);
  aDesiredSize.SetBlockStartAscent(mFirstBaseline);
}

void nsTextControlFrame::Reflow(nsPresContext* aPresContext,
                                ReflowOutput& aDesiredSize,
                                const ReflowInput& aReflowInput,
                                nsReflowStatus& aStatus) {
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
  LogicalSize finalSize(
      wm,
      aReflowInput.ComputedISize() +
          aReflowInput.ComputedLogicalBorderPadding().IStartEnd(wm),
      aReflowInput.ComputedBSize() +
          aReflowInput.ComputedLogicalBorderPadding().BStartEnd(wm));
  aDesiredSize.SetSize(wm, finalSize);

  ComputeBaseline(aReflowInput, aDesiredSize);

  // overflow handling
  aDesiredSize.SetOverflowAreasToDesiredBounds();
  // perform reflow on all kids
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    ReflowTextControlChild(kid, aPresContext, aReflowInput, aStatus,
                           aDesiredSize);
    kid = kid->GetNextSibling();
  }

  // take into account css properties that affect overflow handling
  FinishAndStoreOverflow(&aDesiredSize);

  aStatus.Reset();  // This type of frame can't be split.
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aDesiredSize);
}

void nsTextControlFrame::ReflowTextControlChild(
    nsIFrame* aKid, nsPresContext* aPresContext,
    const ReflowInput& aReflowInput, nsReflowStatus& aStatus,
    ReflowOutput& aParentDesiredSize) {
  // compute available size and frame offsets for child
  WritingMode wm = aKid->GetWritingMode();
  LogicalSize availSize = aReflowInput.ComputedSizeWithPadding(wm);
  availSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;

  ReflowInput kidReflowInput(aPresContext, aReflowInput, aKid, availSize,
                             Nothing(), ReflowInput::CALLER_WILL_INIT);
  // Override padding with our computed padding in case we got it from theming
  // or percentage.
  kidReflowInput.Init(aPresContext, Nothing(), nullptr,
                      &aReflowInput.ComputedPhysicalPadding());

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
  ReflowChild(aKid, aPresContext, desiredSize, kidReflowInput, xOffset, yOffset,
              ReflowChildFlags::Default, aStatus);

  // place the child
  FinishReflowChild(aKid, aPresContext, desiredSize, &kidReflowInput, xOffset,
                    yOffset, ReflowChildFlags::Default);

  // consider the overflow
  aParentDesiredSize.mOverflowAreas.UnionWith(desiredSize.mOverflowAreas);
}

nsSize nsTextControlFrame::GetXULMinSize(nsBoxLayoutState& aState) {
  // XXXbz why?  Why not the nsBoxFrame sizes?
  return nsIFrame::GetXULMinSize(aState);
}

bool nsTextControlFrame::IsXULCollapsed() {
  // We're never collapsed in the box sense.
  return false;
}

NS_IMETHODIMP
nsTextControlFrame::ScrollOnFocusEvent::Run() {
  if (mFrame) {
    TextControlElement* textControlElement =
        TextControlElement::FromNode(mFrame->GetContent());
    MOZ_ASSERT(textControlElement);
    nsISelectionController* selCon =
        textControlElement->GetSelectionController();
    if (selCon) {
      mFrame->mScrollEvent.Forget();
      selCon->ScrollSelectionIntoView(
          nsISelectionController::SELECTION_NORMAL,
          nsISelectionController::SELECTION_FOCUS_REGION,
          nsISelectionController::SCROLL_SYNCHRONOUS);
    }
  }
  return NS_OK;
}

// IMPLEMENTING NS_IFORMCONTROLFRAME
void nsTextControlFrame::SetFocus(bool aOn, bool aRepaint) {
  TextControlElement* textControlElement =
      TextControlElement::FromNode(GetContent());
  MOZ_ASSERT(textControlElement);

  // Revoke the previous scroll event if one exists
  mScrollEvent.Revoke();

  // If 'dom.placeholeder.show_on_focus' preference is 'false', focusing or
  // blurring the frame can have an impact on the placeholder visibility.
  if (mPlaceholderDiv) {
    textControlElement->UpdateOverlayTextVisibility(true);
  }

  if (!aOn) {
    return;
  }

  nsISelectionController* selCon = textControlElement->GetSelectionController();
  if (!selCon) {
    return;
  }

  RefPtr<Selection> ourSel =
      selCon->GetSelection(nsISelectionController::SELECTION_NORMAL);
  if (!ourSel) {
    return;
  }

  mozilla::PresShell* presShell = PresContext()->GetPresShell();
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
    Document* doc = GetContent()->GetComposedDoc();
    if (doc) {
      nsIFocusManager* fm = nsFocusManager::GetFocusManager();
      if (fm) {
        fm->GetLastFocusMethod(doc->GetWindow(), &lastFocusMethod);
      }
    }
    if (!(lastFocusMethod & nsIFocusManager::FLAG_BYMOUSE)) {
      RefPtr<ScrollOnFocusEvent> event = new ScrollOnFocusEvent(this);
      nsresult rv =
          mContent->OwnerDoc()->Dispatch(TaskCategory::Other, do_AddRef(event));
      if (NS_SUCCEEDED(rv)) {
        mScrollEvent = std::move(event);
      }
    }
  }

  // tell the caret to use our selection
  caret->SetSelection(ourSel);

  // mutual-exclusion: the selection is either controlled by the
  // document or by the text input/area. Clear any selection in the
  // document since the focus is now on our independent selection.

  RefPtr<Selection> docSel =
      presShell->GetSelection(nsISelectionController::SELECTION_NORMAL);
  if (!docSel) {
    return;
  }

  if (!docSel->IsCollapsed()) {
    docSel->RemoveAllRanges(IgnoreErrors());
  }
}

nsresult nsTextControlFrame::SetFormProperty(nsAtom* aName,
                                             const nsAString& aValue) {
  if (!mIsProcessing) {  // some kind of lock.
    mIsProcessing = true;
    if (nsGkAtoms::select == aName) {
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

already_AddRefed<TextEditor> nsTextControlFrame::GetTextEditor() {
  if (NS_WARN_IF(NS_FAILED(EnsureEditorInitialized()))) {
    return nullptr;
  }

  RefPtr<TextControlElement> textControlElement =
      TextControlElement::FromNode(GetContent());
  MOZ_ASSERT(textControlElement);
  RefPtr<TextEditor> textEditor = textControlElement->GetTextEditor();
  return textEditor.forget();
}

nsresult nsTextControlFrame::SetSelectionInternal(
    nsINode* aStartNode, uint32_t aStartOffset, nsINode* aEndNode,
    uint32_t aEndOffset, nsITextControlFrame::SelectionDirection aDirection) {
  // Create a new range to represent the new selection.
  // Note that we use a new range to avoid having to do
  // isIncreasing checks to avoid possible errors.

  // Be careful to use internal nsRange methods which do not check to make sure
  // we have access to the node.
  // XXXbz nsRange::SetStartAndEnd takes int32_t (and ranges generally work on
  // int32_t), but we're passing uint32_t.  The good news is that at this point
  // our endpoints should really be within our length, so not really that big.
  // And if they _are_ that big, SetStartAndEnd() will simply error out, which
  // is not too bad for a case we don't expect to happen.
  ErrorResult error;
  RefPtr<nsRange> range =
      nsRange::Create(aStartNode, aStartOffset, aEndNode, aEndOffset, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }
  MOZ_ASSERT(range);

  // Get the selection, clear it and add the new range to it!
  TextControlElement* textControlElement =
      TextControlElement::FromNode(GetContent());
  MOZ_ASSERT(textControlElement);
  nsISelectionController* selCon = textControlElement->GetSelectionController();
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

  selection->RemoveAllRanges(error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  selection->AddRangeAndSelectFramesAndNotifyListeners(
      *range, error);  // NOTE: can destroy the world
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  selection->SetDirection(direction);
  return NS_OK;
}

nsresult nsTextControlFrame::ScrollSelectionIntoView() {
  TextControlElement* textControlElement =
      TextControlElement::FromNode(GetContent());
  MOZ_ASSERT(textControlElement);
  nsISelectionController* selCon = textControlElement->GetSelectionController();
  if (selCon) {
    // Scroll the selection into view (see bug 231389).
    return selCon->ScrollSelectionIntoView(
        nsISelectionController::SELECTION_NORMAL,
        nsISelectionController::SELECTION_FOCUS_REGION,
        nsISelectionController::SCROLL_FIRST_ANCESTOR_ONLY);
  }

  return NS_ERROR_FAILURE;
}

nsresult nsTextControlFrame::SelectAllOrCollapseToEndOfText(bool aSelect) {
  nsresult rv = EnsureEditorInitialized();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsINode> rootNode;
  rootNode = mRootNode;

  NS_ENSURE_TRUE(rootNode, NS_ERROR_FAILURE);

  int32_t numChildren = mRootNode->GetChildCount();

  if (numChildren > 0) {
    // We never want to place the selection after the last
    // br under the root node!
    nsIContent* child = mRootNode->GetLastChild();
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
        numChildren = child->AsText()->TextDataLength();
      }
    }
  }

  rv = SetSelectionInternal(rootNode, aSelect ? 0 : numChildren, rootNode,
                            numChildren);
  NS_ENSURE_SUCCESS(rv, rv);

  return ScrollSelectionIntoView();
}

nsresult nsTextControlFrame::SetSelectionEndPoints(
    uint32_t aSelStart, uint32_t aSelEnd,
    nsITextControlFrame::SelectionDirection aDirection) {
  NS_ASSERTION(aSelStart <= aSelEnd, "Invalid selection offsets!");

  if (aSelStart > aSelEnd) return NS_ERROR_FAILURE;

  nsCOMPtr<nsINode> startNode, endNode;
  uint32_t startOffset, endOffset;

  // Calculate the selection start point.

  nsresult rv =
      OffsetToDOMPoint(aSelStart, getter_AddRefs(startNode), &startOffset);

  NS_ENSURE_SUCCESS(rv, rv);

  if (aSelStart == aSelEnd) {
    // Collapsed selection, so start and end are the same!
    endNode = startNode;
    endOffset = startOffset;
  } else {
    // Selection isn't collapsed so we have to calculate
    // the end point too.

    rv = OffsetToDOMPoint(aSelEnd, getter_AddRefs(endNode), &endOffset);

    NS_ENSURE_SUCCESS(rv, rv);
  }

  return SetSelectionInternal(startNode, startOffset, endNode, endOffset,
                              aDirection);
}

NS_IMETHODIMP
nsTextControlFrame::SetSelectionRange(
    uint32_t aSelStart, uint32_t aSelEnd,
    nsITextControlFrame::SelectionDirection aDirection) {
  nsresult rv = EnsureEditorInitialized();
  NS_ENSURE_SUCCESS(rv, rv);

  if (aSelStart > aSelEnd) {
    // Simulate what we'd see SetSelectionStart() was called, followed
    // by a SetSelectionEnd().

    aSelStart = aSelEnd;
  }

  return SetSelectionEndPoints(aSelStart, aSelEnd, aDirection);
}

nsresult nsTextControlFrame::OffsetToDOMPoint(uint32_t aOffset,
                                              nsINode** aResult,
                                              uint32_t* aPosition) {
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

  NS_ASSERTION(length <= 2,
               "We should have one text node and one mozBR at most");

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
nsresult nsTextControlFrame::AttributeChanged(int32_t aNameSpaceID,
                                              nsAtom* aAttribute,
                                              int32_t aModType) {
  auto* textControlElement = TextControlElement::FromNode(GetContent());
  MOZ_ASSERT(textControlElement);
  nsISelectionController* selCon = textControlElement->GetSelectionController();
  const bool needEditor =
      nsGkAtoms::maxlength == aAttribute || nsGkAtoms::readonly == aAttribute ||
      nsGkAtoms::disabled == aAttribute || nsGkAtoms::spellcheck == aAttribute;
  RefPtr<TextEditor> textEditor = needEditor ? GetTextEditor() : nullptr;
  if ((needEditor && !textEditor) || !selCon) {
    return nsContainerFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                              aModType);
  }

  if (nsGkAtoms::maxlength == aAttribute) {
    if (textEditor) {
      textEditor->SetMaxTextLength(textControlElement->UsedMaxLength());
    }
    return NS_OK;
  }

  if (nsGkAtoms::readonly == aAttribute || nsGkAtoms::disabled == aAttribute) {
    if (AttributeExists(aAttribute)) {
      if (nsContentUtils::IsFocusedContent(mContent)) {
        selCon->SetCaretEnabled(false);
      }
      textEditor->AddFlags(nsIEditor::eEditorReadonlyMask);
    } else {
      if (!AttributeExists(aAttribute == nsGkAtoms::readonly
                               ? nsGkAtoms::disabled
                               : nsGkAtoms::readonly)) {
        if (nsContentUtils::IsFocusedContent(mContent)) {
          selCon->SetCaretEnabled(true);
        }
        textEditor->RemoveFlags(nsIEditor::eEditorReadonlyMask);
      }
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

void nsTextControlFrame::GetText(nsString& aText) {
  if (HTMLInputElement* inputElement = HTMLInputElement::FromNode(mContent)) {
    if (IsSingleLineTextControl()) {
      // There will be no line breaks so we can ignore the wrap property.
      inputElement->GetTextEditorValue(aText, true);
      return;
    }
    aText.Truncate();
    return;
  }

  MOZ_ASSERT(!IsSingleLineTextControl());
  if (HTMLTextAreaElement* textAreaElement =
          HTMLTextAreaElement::FromNode(mContent)) {
    textAreaElement->GetValue(aText);
    return;
  }

  MOZ_ASSERT(aText.IsEmpty());
  aText.Truncate();
}

bool nsTextControlFrame::TextEquals(const nsAString& aText) const {
  if (HTMLInputElement* inputElement = HTMLInputElement::FromNode(mContent)) {
    if (IsSingleLineTextControl()) {
      // There will be no line breaks so we can ignore the wrap property.
      return inputElement->TextEditorValueEquals(aText);
    }
    return aText.IsEmpty();
  }

  MOZ_ASSERT(!IsSingleLineTextControl());
  if (HTMLTextAreaElement* textAreaElement =
          HTMLTextAreaElement::FromNode(mContent)) {
    return textAreaElement->ValueEquals(aText);
  }

  return aText.IsEmpty();
}

/// END NSIFRAME OVERLOADS

// NOTE(emilio): This is needed because the root->primary frame map is not set
// up by the time this is called.
static nsIFrame* FindRootNodeFrame(const nsFrameList& aChildList,
                                   const nsIContent* aRoot) {
  for (nsIFrame* f : aChildList) {
    if (f->GetContent() == aRoot) {
      return f;
    }
    if (nsIFrame* root = FindRootNodeFrame(f->PrincipalChildList(), aRoot)) {
      return root;
    }
  }
  return nullptr;
}

void nsTextControlFrame::SetInitialChildList(ChildListID aListID,
                                             nsFrameList& aChildList) {
  nsContainerFrame::SetInitialChildList(aListID, aChildList);
  if (aListID != kPrincipalList) {
    return;
  }

  // Mark the scroll frame as being a reflow root. This will allow incremental
  // reflows to be initiated at the scroll frame, rather than descending from
  // the root frame of the frame hierarchy.
  if (nsIFrame* frame = FindRootNodeFrame(PrincipalChildList(), mRootNode)) {
    frame->AddStateBits(NS_FRAME_REFLOW_ROOT);

    auto* textControlElement = TextControlElement::FromNode(GetContent());
    MOZ_ASSERT(textControlElement);
    textControlElement->InitializeKeyboardEventListeners();

    if (nsPoint* contentScrollPos = TakeProperty(ContentScrollPos())) {
      // If we have a scroll pos stored to be passed to our anonymous
      // div, do it here!
      nsIStatefulFrame* statefulFrame = do_QueryFrame(frame);
      NS_ASSERTION(statefulFrame,
                   "unexpected type of frame for the anonymous div");
      UniquePtr<PresState> fakePresState = NewPresState();
      fakePresState->scrollState() = *contentScrollPos;
      statefulFrame->RestoreState(fakePresState.get());
      delete contentScrollPos;
    }
  } else {
    MOZ_ASSERT(!mRootNode || PrincipalChildList().IsEmpty());
  }
}

void nsTextControlFrame::SetValueChanged(bool aValueChanged) {
  auto* textControlElement = TextControlElement::FromNode(GetContent());
  MOZ_ASSERT(textControlElement);

  if (mPlaceholderDiv) {
    AutoWeakFrame weakFrame(this);
    textControlElement->UpdateOverlayTextVisibility(true);
    if (!weakFrame.IsAlive()) {
      return;
    }
  }

  textControlElement->SetValueChanged(aValueChanged);
}

nsresult nsTextControlFrame::UpdateValueDisplay(bool aNotify,
                                                bool aBeforeEditorInit,
                                                const nsAString* aValue) {
  if (!IsSingleLineTextControl())  // textareas don't use this
    return NS_OK;

  MOZ_ASSERT(mRootNode, "Must have a div content\n");
  MOZ_ASSERT(!mEditorHasBeenInitialized,
             "Do not call this after editor has been initialized");

  nsIContent* childContent = mRootNode->GetFirstChild();
  Text* textContent;
  if (!childContent) {
    // Set up a textnode with our value
    RefPtr<nsTextNode> textNode = new (mContent->NodeInfo()->NodeInfoManager())
        nsTextNode(mContent->NodeInfo()->NodeInfoManager());
    textNode->MarkAsMaybeModifiedFrequently();
    if (IsPasswordTextControl()) {
      textNode->MarkAsMaybeMasked();
    }

    mRootNode->AppendChildTo(textNode, aNotify);
    textContent = textNode;
  } else {
    textContent = childContent->GetAsText();
  }

  NS_ENSURE_TRUE(textContent, NS_ERROR_UNEXPECTED);

  TextControlElement* textControlElement =
      TextControlElement::FromNode(GetContent());
  MOZ_ASSERT(textControlElement);

  // Get the current value of the textfield from the content.
  nsAutoString value;
  if (aValue) {
    value = *aValue;
  } else {
    textControlElement->GetTextEditorValue(value, true);
  }

  // Update the display of the placeholder value and preview text if needed.
  // We don't need to do this if we're about to initialize the editor, since
  // EnsureEditorInitialized takes care of this.
  if ((mPlaceholderDiv || mPreviewDiv) && !aBeforeEditorInit) {
    AutoWeakFrame weakFrame(this);
    textControlElement->UpdateOverlayTextVisibility(aNotify);
    NS_ENSURE_STATE(weakFrame.IsAlive());
  }

  if (aBeforeEditorInit && value.IsEmpty()) {
    if (nsIContent* node = mRootNode->GetFirstChild()) {
      mRootNode->RemoveChildNode(node, true);
    }
    return NS_OK;
  }

  return textContent->SetText(value, aNotify);
}

NS_IMETHODIMP
nsTextControlFrame::GetOwnedSelectionController(
    nsISelectionController** aSelCon) {
  NS_ENSURE_ARG_POINTER(aSelCon);

  TextControlElement* textControlElement =
      TextControlElement::FromNode(GetContent());
  MOZ_ASSERT(textControlElement);

  *aSelCon = textControlElement->GetSelectionController();
  NS_IF_ADDREF(*aSelCon);

  return NS_OK;
}

nsFrameSelection* nsTextControlFrame::GetOwnedFrameSelection() {
  auto* textControlElement = TextControlElement::FromNode(GetContent());
  MOZ_ASSERT(textControlElement);
  return textControlElement->GetConstFrameSelection();
}

UniquePtr<PresState> nsTextControlFrame::SaveState() {
  if (nsIStatefulFrame* scrollStateFrame =
          do_QueryFrame(GetScrollTargetFrame())) {
    return scrollStateFrame->SaveState();
  }

  return nullptr;
}

NS_IMETHODIMP
nsTextControlFrame::RestoreState(PresState* aState) {
  NS_ENSURE_ARG_POINTER(aState);

  // Query the nsIStatefulFrame from the HTMLScrollFrame
  if (nsIStatefulFrame* scrollStateFrame =
          do_QueryFrame(GetScrollTargetFrame())) {
    return scrollStateFrame->RestoreState(aState);
  }

  // Most likely, we don't have our anonymous content constructed yet, which
  // would cause us to end up here.  In this case, we'll just store the scroll
  // pos ourselves, and forward it to the scroll frame later when it's created.
  SetProperty(ContentScrollPos(), new nsPoint(aState->scrollState()));
  return NS_OK;
}

nsresult nsTextControlFrame::PeekOffset(nsPeekOffsetStruct* aPos) {
  return NS_ERROR_FAILURE;
}

void nsTextControlFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                          const nsDisplayListSet& aLists) {
  /*
   * The implementation of this method is equivalent as:
   * nsContainerFrame::BuildDisplayList()
   * with the difference that we filter-out the placeholder frame when it
   * should not be visible.
   */
  DO_GLOBAL_REFLOW_COUNT_DSP("nsTextControlFrame");

  auto* control = TextControlElement::FromNode(GetContent());
  MOZ_ASSERT(control);

  DisplayBorderBackgroundOutline(aBuilder, aLists);

  // Redirect all lists to the Content list so that nothing can escape, ie
  // opacity creating stacking contexts that then get sorted with stacking
  // contexts external to us.
  nsDisplayList* content = aLists.Content();
  nsDisplayListSet set(content, content, content, content, content, content);

  for (nsIFrame* kid = mFrames.FirstChild(); kid; kid = kid->GetNextSibling()) {
    nsIContent* kidContent = kid->GetContent();
    Maybe<DisplayListClipState::AutoSaveRestore> overlayTextClip;
    if (kidContent == mPlaceholderDiv && !control->GetPlaceholderVisibility()) {
      continue;
    }
    if (kidContent == mPreviewDiv && !control->GetPreviewVisibility()) {
      continue;
    }
    // Clip the preview text to the root box, so that it doesn't, e.g., overlay
    // with our <input type="number"> spin buttons.
    //
    // For other input types, this will be a noop since we size the root via
    // ReflowTextControlChild, which sets the same available space for all
    // children.
    if (kidContent == mPlaceholderDiv || kidContent == mPreviewDiv) {
      if (mRootNode) {
        if (auto* root = mRootNode->GetPrimaryFrame()) {
          overlayTextClip.emplace(aBuilder);
          nsRect rootBox(aBuilder->ToReferenceFrame(root), root->GetSize());
          overlayTextClip->ClipContentDescendants(rootBox);
        }
      }
    }
    BuildDisplayListForChild(aBuilder, kid, set, 0);
  }
}

NS_IMETHODIMP
nsTextControlFrame::EditorInitializer::Run() {
  if (!mFrame) {
    return NS_OK;
  }

  // Need to block script to avoid bug 669767.
  nsAutoScriptBlocker scriptBlocker;

  RefPtr<mozilla::PresShell> presShell = mFrame->PresShell();
  bool observes = presShell->ObservesNativeAnonMutationsForPrint();
  presShell->ObserveNativeAnonMutationsForPrint(true);
  // This can cause the frame to be destroyed (and call Revoke()).
  mFrame->EnsureEditorInitialized();
  presShell->ObserveNativeAnonMutationsForPrint(observes);

  // The frame can *still* be destroyed even though we have a scriptblocker,
  // bug 682684.
  if (!mFrame) {
    return NS_ERROR_FAILURE;
  }

  mFrame->FinishedInitializer();
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsTextControlFrame::nsAnonDivObserver, nsIMutationObserver)

void nsTextControlFrame::nsAnonDivObserver::CharacterDataChanged(
    nsIContent* aContent, const CharacterDataChangeInfo&) {
  mFrame.ClearCachedValue();
}

void nsTextControlFrame::nsAnonDivObserver::ContentAppended(
    nsIContent* aFirstNewContent) {
  mFrame.ClearCachedValue();
}

void nsTextControlFrame::nsAnonDivObserver::ContentInserted(
    nsIContent* aChild) {
  mFrame.ClearCachedValue();
}

void nsTextControlFrame::nsAnonDivObserver::ContentRemoved(
    nsIContent* aChild, nsIContent* aPreviousSibling) {
  mFrame.ClearCachedValue();
}
