/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Blake Ross <blakeross@telocity.com>
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


#include "nsCOMPtr.h"
#include "nsTextControlFrame.h"
#include "nsIDocument.h"
#include "nsIDOMNSHTMLTextAreaElement.h"
#include "nsIFormControl.h"
#include "nsIServiceManager.h"
#include "nsFrameSelection.h"
#include "nsIPlaintextEditor.h"
#include "nsEditorCID.h"
#include "nsLayoutCID.h"
#include "nsIDocumentEncoder.h"
#include "nsCaret.h"
#include "nsISelectionListener.h"
#include "nsISelectionPrivate.h"
#include "nsIController.h"
#include "nsIControllers.h"
#include "nsIControllerContext.h"
#include "nsGenericHTMLElement.h"
#include "nsIEditorIMESupport.h"
#include "nsIPhonetic.h"
#include "nsIEditorObserver.h"
#include "nsEditProperty.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsINameSpaceManager.h"
#include "nsINodeInfo.h"
#include "nsFormControlFrame.h" //for registering accesskeys
#include "nsIDeviceContext.h" // to measure fonts

#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsLayoutUtils.h"
#include "nsIComponentManager.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIPresShell.h"
#include "nsIComponentManager.h"

#include "nsBoxLayoutState.h"
//for keylistener for "return" check
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDocument.h" //observe documents to send onchangenotifications
#include "nsIStyleSheet.h"//observe documents to send onchangenotifications
#include "nsIStyleRule.h"//observe documents to send onchangenotifications
#include "nsIDOMEventListener.h"//observe documents to send onchangenotifications
#include "nsGUIEvent.h"
#include "nsIDOMEventGroup.h"
#include "nsIDOM3EventTarget.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIEventStateManager.h"

#include "nsIDOMFocusListener.h" //onchange events
#include "nsIDOMCharacterData.h" //for selection setting helper func
#include "nsIDOMNodeList.h" //for selection setting helper func
#include "nsIDOMRange.h" //for selection setting helper func
#include "nsPIDOMWindow.h" //needed for notify selection changed to update the menus ect.
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif
#include "nsIServiceManager.h"
#include "nsIDOMNode.h"

#include "nsIEditorObserver.h"
#include "nsITransactionManager.h"
#include "nsIDOMText.h" //for multiline getselection
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMEventGroup.h"
#include "nsIDOM3EventTarget.h"
#include "nsINativeKeyBindings.h"
#include "nsIJSContextStack.h"
#include "nsFocusManager.h"
#include "nsTextEditRules.h"
#include "nsIFontMetrics.h"
#include "nsIDOMNSHTMLElement.h"

#include "mozilla/FunctionTimer.h"

#define DEFAULT_COLUMN_WIDTH 20

#include "nsContentCID.h"
static NS_DEFINE_IID(kRangeCID,     NS_RANGE_CID);

nsIFrame*
NS_NewTextControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsTextControlFrame(aPresShell, aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsTextControlFrame)

NS_QUERYFRAME_HEAD(nsTextControlFrame)
  NS_QUERYFRAME_ENTRY(nsIFormControlFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
  NS_QUERYFRAME_ENTRY(nsITextControlFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsBoxFrame)

#ifdef ACCESSIBILITY
already_AddRefed<nsAccessible>
nsTextControlFrame::CreateAccessible()
{
  nsCOMPtr<nsIAccessibilityService> accService = do_GetService("@mozilla.org/accessibilityService;1");

  if (accService) {
    return accService->CreateHTMLTextFieldAccessible(mContent,
                                                     PresContext()->PresShell());
  }

  return nsnull;
}
#endif

#ifdef DEBUG
class EditorInitializerEntryTracker {
public:
  explicit EditorInitializerEntryTracker(nsTextControlFrame &frame)
    : mFrame(frame)
    , mFirstEntry(PR_FALSE)
  {
    if (!mFrame.mInEditorInitialization) {
      mFrame.mInEditorInitialization = PR_TRUE;
      mFirstEntry = PR_TRUE;
    }
  }
  ~EditorInitializerEntryTracker()
  {
    if (mFirstEntry) {
      mFrame.mInEditorInitialization = PR_FALSE;
    }
  }
  PRBool EnteredMoreThanOnce() const { return !mFirstEntry; }
private:
  nsTextControlFrame &mFrame;
  PRBool mFirstEntry;
};
#endif

nsTextControlFrame::nsTextControlFrame(nsIPresShell* aShell, nsStyleContext* aContext)
  : nsStackFrame(aShell, aContext)
  , mUseEditor(PR_FALSE)
  , mIsProcessing(PR_FALSE)
  , mNotifyOnInput(PR_TRUE)
  , mFireChangeEventState(PR_FALSE)
  , mInSecureKeyboardInputMode(PR_FALSE)
#ifdef DEBUG
  , mInEditorInitialization(PR_FALSE)
#endif
{
}

nsTextControlFrame::~nsTextControlFrame()
{
}

void
nsTextControlFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  if (mInSecureKeyboardInputMode) {
    MaybeEndSecureKeyboardInput();
  }

  mScrollEvent.Revoke();

  // Unbind the text editor state object from the frame.  The editor will live
  // on, but things like controllers will be released.
  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element");
  txtCtrl->UnbindFromFrame(this);

  nsFormControlFrame::RegUnRegAccessKey(static_cast<nsIFrame*>(this), PR_FALSE);

  nsBoxFrame::DestroyFrom(aDestructRoot);
}

nsIAtom*
nsTextControlFrame::GetType() const 
{ 
  return nsGkAtoms::textInputFrame;
} 

nsresult nsTextControlFrame::MaybeBeginSecureKeyboardInput()
{
  nsresult rv = NS_OK;
  if (IsPasswordTextControl() && !mInSecureKeyboardInputMode) {
    nsIWidget* window = GetNearestWidget();
    NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);
    rv = window->BeginSecureKeyboardInput();
    mInSecureKeyboardInputMode = NS_SUCCEEDED(rv);
  }
  return rv;
}

void nsTextControlFrame::MaybeEndSecureKeyboardInput()
{
  if (mInSecureKeyboardInputMode) {
    nsIWidget* window = GetNearestWidget();
    if (!window)
      return;
    window->EndSecureKeyboardInput();
    mInSecureKeyboardInputMode = PR_FALSE;
  }
}

nsresult
nsTextControlFrame::CalcIntrinsicSize(nsIRenderingContext* aRenderingContext,
                                      nsSize&              aIntrinsicSize)
{
  // Get leading and the Average/MaxAdvance char width 
  nscoord lineHeight  = 0;
  nscoord charWidth   = 0;
  nscoord charMaxAdvance  = 0;

  nsCOMPtr<nsIFontMetrics> fontMet;
  nsresult rv =
    nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fontMet));
  NS_ENSURE_SUCCESS(rv, rv);
  aRenderingContext->SetFont(fontMet);

  lineHeight =
    nsHTMLReflowState::CalcLineHeight(GetStyleContext(), NS_AUTOHEIGHT);
  fontMet->GetAveCharWidth(charWidth);
  fontMet->GetMaxAdvance(charMaxAdvance);

  // Set the width equal to the width in characters
  PRInt32 cols = GetCols();
  aIntrinsicSize.width = cols * charWidth;

  // To better match IE, take the maximum character width(in twips) and remove
  // 4 pixels add this on as additional padding(internalPadding). But only do
  // this if charMaxAdvance != charWidth; if they are equal, this is almost
  // certainly a fixed-width font.
  if (charWidth != charMaxAdvance) {
    nscoord internalPadding = NS_MAX(0, charMaxAdvance -
                                        nsPresContext::CSSPixelsToAppUnits(4));
    nscoord t = nsPresContext::CSSPixelsToAppUnits(1); 
   // Round to a multiple of t
    nscoord rest = internalPadding % t; 
    if (rest < t - rest) {
      internalPadding -= rest;
    } else {
      internalPadding += t - rest;
    }
    // Now add the extra padding on (so that small input sizes work well)
    aIntrinsicSize.width += internalPadding;
  } else {
    // This is to account for the anonymous <br> having a 1 twip width
    // in Full Standards mode, see BRFrame::Reflow and bug 228752.
    if (PresContext()->CompatibilityMode() == eCompatibility_FullStandards) {
      aIntrinsicSize.width += 1;
    }

    // Also add in the padding of our value div child.  Note that it hasn't
    // been reflowed yet, so we can't get its used padding, but it shouldn't be
    // using percentage padding anyway.
    nsMargin childPadding;
    nsIFrame* firstChild = GetFirstChild(nsnull);
    if (firstChild && firstChild->GetStylePadding()->GetPadding(childPadding)) {
      aIntrinsicSize.width += childPadding.LeftRight();
    } else {
      NS_ERROR("Percentage padding on value div?");
    }
  }

  // Increment width with cols * letter-spacing.
  {
    const nsStyleCoord& lsCoord = GetStyleText()->mLetterSpacing;
    if (eStyleUnit_Coord == lsCoord.GetUnit()) {
      nscoord letterSpacing = lsCoord.GetCoordValue();
      if (letterSpacing != 0) {
        aIntrinsicSize.width += cols * letterSpacing;
      }
    }
  }

  // Set the height equal to total number of rows (times the height of each
  // line, of course)
  aIntrinsicSize.height = lineHeight * GetRows();

  // Add in the size of the scrollbars for textarea
  if (IsTextArea()) {
    nsIFrame* first = GetFirstChild(nsnull);

    nsIScrollableFrame *scrollableFrame = do_QueryFrame(first);
    NS_ASSERTION(scrollableFrame, "Child must be scrollable");

    if (scrollableFrame) {
      nsMargin scrollbarSizes =
      scrollableFrame->GetDesiredScrollbarSizes(PresContext(), aRenderingContext);

      aIntrinsicSize.width  += scrollbarSizes.LeftRight();

      aIntrinsicSize.height += scrollbarSizes.TopBottom();;
    }
  }

  return NS_OK;
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

  // Check if this method has been called already.
  // If so, just return early.
  if (mUseEditor)
    return NS_OK;

  NS_TIME_FUNCTION;

  nsIDocument* doc = mContent->GetCurrentDoc();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  nsWeakFrame weakFrame(this);

  // Flush out content on our document.  Have to do this, because script
  // blockers don't prevent the sink flushing out content and notifying in the
  // process, which can destroy frames.
  doc->FlushPendingNotifications(Flush_ContentAndNotify);
  NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_ERROR_FAILURE);

  // Make sure that editor init doesn't do things that would kill us off
  // (especially off the script blockers it'll create for its DOM mutations).
  nsAutoScriptBlocker scriptBlocker;

  // Time to mess with our security context... See comments in GetValue()
  // for why this is needed.
  nsCxPusher pusher;
  pusher.PushNull();

  // Make sure that we try to focus the content even if the method fails
  class EnsureSetFocus {
  public:
    explicit EnsureSetFocus(nsTextControlFrame* aFrame)
      : mFrame(aFrame) {}
    ~EnsureSetFocus() {
      if (nsContentUtils::IsFocusedContent(mFrame->GetContent()))
        mFrame->SetFocus(PR_TRUE, PR_FALSE);
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
  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element");
  nsresult rv = txtCtrl->CreateEditor();
  NS_ENSURE_SUCCESS(rv, rv);

  // Turn on mUseEditor so that subsequent calls will use the
  // editor.
  mUseEditor = PR_TRUE;

  return NS_OK;
}

nsresult
nsTextControlFrame::CreateAnonymousContent(nsTArray<nsIContent*>& aElements)
{
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

  nsIContent* placeholderNode = txtCtrl->GetPlaceholderNode();
  NS_ENSURE_TRUE(placeholderNode, NS_ERROR_OUT_OF_MEMORY);

  if (!aElements.AppendElement(placeholderNode))
    return NS_ERROR_OUT_OF_MEMORY;

  rv = UpdateValueDisplay(PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  // textareas are eagerly initialized
  PRBool initEagerly = !IsSingleLineTextControl();
  if (!initEagerly) {
    nsCOMPtr<nsIDOMNSHTMLElement> element = do_QueryInterface(txtCtrl);
    if (element) {
      // so are input text controls with spellcheck=true
      element->GetSpellcheck(&initEagerly);
    }
  }

  if (initEagerly) {
    NS_ASSERTION(!nsContentUtils::IsSafeToRunScript(),
                 "Someone forgot a script blocker?");

    if (!nsContentUtils::AddScriptRunner(new EditorInitializer(this))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return NS_OK;
}

void
nsTextControlFrame::AppendAnonymousContentTo(nsBaseContentList& aElements)
{
  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element");

  aElements.MaybeAppendElement(txtCtrl->GetRootEditorNode());
  aElements.MaybeAppendElement(txtCtrl->GetPlaceholderNode());
}

nscoord
nsTextControlFrame::GetMinWidth(nsIRenderingContext* aRenderingContext)
{
  // Our min width is just our preferred width if we have auto width.
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);

  result = GetPrefWidth(aRenderingContext);

  return result;
}

nsSize
nsTextControlFrame::ComputeAutoSize(nsIRenderingContext *aRenderingContext,
                                    nsSize aCBSize, nscoord aAvailableWidth,
                                    nsSize aMargin, nsSize aBorder,
                                    nsSize aPadding, PRBool aShrinkWrap)
{
  nsSize autoSize;
  nsresult rv = CalcIntrinsicSize(aRenderingContext, autoSize);
  if (NS_FAILED(rv)) {
    // What now?
    autoSize.SizeTo(0, 0);
  }
#ifdef DEBUG
  // Note: Ancestor ComputeAutoSize only computes a width if we're auto-width
  else if (GetStylePosition()->mWidth.GetUnit() == eStyleUnit_Auto) {
    nsSize ancestorAutoSize =
      nsStackFrame::ComputeAutoSize(aRenderingContext,
                                    aCBSize, aAvailableWidth,
                                    aMargin, aBorder,
                                    aPadding, aShrinkWrap);
    NS_ASSERTION(ancestorAutoSize.width == autoSize.width,
                 "Incorrect size computed by ComputeAutoSize?");
  }
#endif
  
  return autoSize;
}


// We inherit our GetPrefWidth from nsBoxFrame

NS_IMETHODIMP
nsTextControlFrame::Reflow(nsPresContext*   aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsTextControlFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  // make sure the the form registers itself on the initial/first reflow
  if (mState & NS_FRAME_FIRST_REFLOW) {
    nsFormControlFrame::RegUnRegAccessKey(this, PR_TRUE);
  }

  return nsStackFrame::Reflow(aPresContext, aDesiredSize, aReflowState,
                              aStatus);
}

nsSize
nsTextControlFrame::GetPrefSize(nsBoxLayoutState& aState)
{
  if (!DoesNeedRecalc(mPrefSize))
     return mPrefSize;

#ifdef DEBUG_LAYOUT
  PropagateDebug(aState);
#endif

  nsSize pref(0,0);

  nsresult rv = CalcIntrinsicSize(aState.GetRenderingContext(), pref);
  NS_ENSURE_SUCCESS(rv, pref);
  AddBorderAndPadding(pref);

  PRBool widthSet, heightSet;
  nsIBox::AddCSSPrefSize(this, pref, widthSet, heightSet);

  nsSize minSize = GetMinSize(aState);
  nsSize maxSize = GetMaxSize(aState);
  mPrefSize = BoundsCheck(minSize, pref, maxSize);

#ifdef DEBUG_rods
  {
    nsMargin borderPadding(0,0,0,0);
    GetBorderAndPadding(borderPadding);
    nsSize size(169, 24);
    nsSize actual(pref.width/15, 
                  pref.height/15);
    printf("nsGfxText(field) %d,%d  %d,%d  %d,%d\n", 
           size.width, size.height, actual.width, actual.height, actual.width-size.width, actual.height-size.height);  // text field
  }
#endif

  return mPrefSize;
}

nsSize
nsTextControlFrame::GetMinSize(nsBoxLayoutState& aState)
{
  // XXXbz why?  Why not the nsBoxFrame sizes?
  return nsBox::GetMinSize(aState);
}

nsSize
nsTextControlFrame::GetMaxSize(nsBoxLayoutState& aState)
{
  // XXXbz why?  Why not the nsBoxFrame sizes?
  return nsBox::GetMaxSize(aState);
}

nscoord
nsTextControlFrame::GetBoxAscent(nsBoxLayoutState& aState)
{
  // Return the baseline of the first (nominal) row, with centering for
  // single-line controls.

  // First calculate the ascent wrt the client rect
  nsRect clientRect;
  GetClientRect(clientRect);
  nscoord lineHeight =
    IsSingleLineTextControl() ? clientRect.height :
    nsHTMLReflowState::CalcLineHeight(GetStyleContext(), NS_AUTOHEIGHT);

  nsCOMPtr<nsIFontMetrics> fontMet;
  nsresult rv =
    nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fontMet));
  NS_ENSURE_SUCCESS(rv, 0);

  nscoord ascent = nsLayoutUtils::GetCenteredFontBaseline(fontMet, lineHeight);

  // Now adjust for our borders and padding
  ascent += clientRect.y;

  return ascent;
}

PRBool
nsTextControlFrame::IsCollapsed(nsBoxLayoutState& aBoxLayoutState)
{
  // We're never collapsed in the box sense.
  return PR_FALSE;
}

PRBool
nsTextControlFrame::IsLeaf() const
{
  return PR_TRUE;
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
                                      PR_TRUE);
    }
  }
  return NS_OK;
}

//IMPLEMENTING NS_IFORMCONTROLFRAME
void nsTextControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element");

  // Revoke the previous scroll event if one exists
  mScrollEvent.Revoke();

  if (!aOn) {
    nsWeakFrame weakFrame(this);

    PRInt32 length;
    nsresult rv = GetTextLength(&length);
    NS_ENSURE_SUCCESS(rv, );
    if (!length)
      txtCtrl->SetPlaceholderClass(PR_TRUE, PR_TRUE);

    if (!weakFrame.IsAlive())
    {
      return;
    }

    MaybeEndSecureKeyboardInput();
    return;
  }

  nsISelectionController* selCon = txtCtrl->GetSelectionController();
  if (!selCon)
    return;

  nsWeakFrame weakFrame(this);

  txtCtrl->SetPlaceholderClass(PR_FALSE, PR_TRUE);

  if (!weakFrame.IsAlive())
  {
    return;
  }

  if (NS_SUCCEEDED(InitFocusedValue()))
    MaybeBeginSecureKeyboardInput();

  nsCOMPtr<nsISelection> ourSel;
  selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, 
    getter_AddRefs(ourSel));
  if (!ourSel) return;

  nsIPresShell* presShell = PresContext()->GetPresShell();
  nsRefPtr<nsCaret> caret = presShell->GetCaret();
  if (!caret) return;

  // Scroll the current selection into view
  nsISelection *caretSelection = caret->GetCaretDOMSelection();
  const PRBool isFocusedRightNow = ourSel == caretSelection;
  if (!isFocusedRightNow) {
    nsRefPtr<ScrollOnFocusEvent> event = new ScrollOnFocusEvent(this);
    nsresult rv = NS_DispatchToCurrentThread(event);
    if (NS_SUCCEEDED(rv)) {
      mScrollEvent = event;
    }
  }

  // tell the caret to use our selection
  caret->SetCaretDOMSelection(ourSel);

  // mutual-exclusion: the selection is either controlled by the
  // document or by the text input/area. Clear any selection in the
  // document since the focus is now on our independent selection.

  nsCOMPtr<nsISelectionController> selcon = do_QueryInterface(presShell);
  nsCOMPtr<nsISelection> docSel;
  selcon->GetSelection(nsISelectionController::SELECTION_NORMAL,
    getter_AddRefs(docSel));
  if (!docSel) return;

  PRBool isCollapsed = PR_FALSE;
  docSel->GetIsCollapsed(&isCollapsed);
  if (!isCollapsed)
    docSel->RemoveAllRanges();
}

nsresult nsTextControlFrame::SetFormProperty(nsIAtom* aName, const nsAString& aValue)
{
  if (!mIsProcessing)//some kind of lock.
  {
    mIsProcessing = PR_TRUE;
    if (nsGkAtoms::select == aName)
    {
      // Select all the text.
      //
      // XXX: This is lame, we can't call editor's SelectAll method
      //      because that triggers AutoCopies in unix builds.
      //      Instead, we have to call our own homegrown version
      //      of select all which merely builds a range that selects
      //      all of the content and adds that to the selection.

      SelectAllOrCollapseToEndOfText(PR_TRUE);
    }
    mIsProcessing = PR_FALSE;
  }
  return NS_OK;
}

nsresult
nsTextControlFrame::GetFormProperty(nsIAtom* aName, nsAString& aValue) const
{
  NS_ASSERTION(nsGkAtoms::value != aName,
               "Should get the value from the content node instead");
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

NS_IMETHODIMP
nsTextControlFrame::GetTextLength(PRInt32* aTextLength)
{
  NS_ENSURE_ARG_POINTER(aTextLength);

  nsAutoString   textContents;
  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element");
  txtCtrl->GetTextEditorValue(textContents, PR_FALSE);   // this is expensive!
  *aTextLength = textContents.Length();
  return NS_OK;
}

nsresult
nsTextControlFrame::SetSelectionInternal(nsIDOMNode *aStartNode,
                                         PRInt32 aStartOffset,
                                         nsIDOMNode *aEndNode,
                                         PRInt32 aEndOffset)
{
  // Create a new range to represent the new selection.
  // Note that we use a new range to avoid having to do
  // isIncreasing checks to avoid possible errors.

  nsCOMPtr<nsIDOMRange> range = do_CreateInstance(kRangeCID);
  NS_ENSURE_TRUE(range, NS_ERROR_FAILURE);

  nsresult rv = range->SetStart(aStartNode, aStartOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = range->SetEnd(aEndNode, aEndOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the selection, clear it and add the new range to it!
  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element");
  nsISelectionController* selCon = txtCtrl->GetSelectionController();
  NS_ENSURE_TRUE(selCon, NS_ERROR_FAILURE);

  nsCOMPtr<nsISelection> selection;
  selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));  
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);

  rv = selection->RemoveAllRanges();  

  NS_ENSURE_SUCCESS(rv, rv);

  rv = selection->AddRange(range);
  NS_ENSURE_SUCCESS(rv, rv);

  // Scroll the selection into view (see bug 231389)
  return selCon->ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL,
                                         nsISelectionController::SELECTION_FOCUS_REGION,
                                         PR_FALSE);
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
nsTextControlFrame::SelectAllOrCollapseToEndOfText(PRBool aSelect)
{
  nsCOMPtr<nsIDOMElement> rootElement;
  nsresult rv = GetRootNodeAndInitializeEditor(getter_AddRefs(rootElement));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContent> rootContent = do_QueryInterface(rootElement);
  nsCOMPtr<nsIDOMNode> rootNode(do_QueryInterface(rootElement));
  PRInt32 numChildren = rootContent->GetChildCount();

  if (numChildren > 0) {
    // We never want to place the selection after the last
    // br under the root node!
    nsIContent *child = rootContent->GetChildAt(numChildren - 1);
    if (child) {
      if (child->Tag() == nsGkAtoms::br)
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

  return SetSelectionInternal(rootNode, aSelect ? 0 : numChildren,
                              rootNode, numChildren);
}

nsresult
nsTextControlFrame::SetSelectionEndPoints(PRInt32 aSelStart, PRInt32 aSelEnd)
{
  NS_ASSERTION(aSelStart <= aSelEnd, "Invalid selection offsets!");

  if (aSelStart > aSelEnd)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNode> startNode, endNode;
  PRInt32 startOffset, endOffset;

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

  return SetSelectionInternal(startNode, startOffset, endNode, endOffset);
}

NS_IMETHODIMP
nsTextControlFrame::SetSelectionRange(PRInt32 aSelStart, PRInt32 aSelEnd)
{
  nsresult rv = EnsureEditorInitialized();
  NS_ENSURE_SUCCESS(rv, rv);

  if (aSelStart > aSelEnd) {
    // Simulate what we'd see SetSelectionStart() was called, followed
    // by a SetSelectionEnd().

    aSelStart   = aSelEnd;
  }

  return SetSelectionEndPoints(aSelStart, aSelEnd);
}


NS_IMETHODIMP
nsTextControlFrame::SetSelectionStart(PRInt32 aSelectionStart)
{
  nsresult rv = EnsureEditorInitialized();
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 selStart = 0, selEnd = 0; 

  rv = GetSelectionRange(&selStart, &selEnd);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aSelectionStart > selEnd) {
    // Collapse to the new start point.
    selEnd = aSelectionStart; 
  }

  selStart = aSelectionStart;
  
  return SetSelectionEndPoints(selStart, selEnd);
}

NS_IMETHODIMP
nsTextControlFrame::SetSelectionEnd(PRInt32 aSelectionEnd)
{
  nsresult rv = EnsureEditorInitialized();
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 selStart = 0, selEnd = 0; 

  rv = GetSelectionRange(&selStart, &selEnd);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aSelectionEnd < selStart) {
    // Collapse to the new end point.
    selStart = aSelectionEnd; 
  }

  selEnd = aSelectionEnd;
  
  return SetSelectionEndPoints(selStart, selEnd);
}

nsresult
nsTextControlFrame::DOMPointToOffset(nsIDOMNode* aNode,
                                     PRInt32 aNodeOffset,
                                     PRInt32* aResult)
{
  NS_ENSURE_ARG_POINTER(aNode && aResult);

  *aResult = 0;

  nsCOMPtr<nsIDOMElement> rootElement;
  nsresult rv = GetRootNodeAndInitializeEditor(getter_AddRefs(rootElement));
  nsCOMPtr<nsIDOMNode> rootNode(do_QueryInterface(rootElement));

  NS_ENSURE_TRUE(rootNode, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMNodeList> nodeList;

  rv = rootNode->GetChildNodes(getter_AddRefs(nodeList));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(nodeList, NS_ERROR_FAILURE);

  PRUint32 length = 0;
  rv = nodeList->GetLength(&length);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!length || aNodeOffset < 0)
    return NS_OK;

  NS_ASSERTION(length <= 2, "We should have one text node and one mozBR at most");

  nsCOMPtr<nsIDOMNode> firstNode;
  rv = nodeList->Item(0, getter_AddRefs(firstNode));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(firstNode);

  nsCOMPtr<nsIDOMText> nodeAsText = do_QueryInterface(aNode);
  if (nodeAsText || (aNode == rootNode && aNodeOffset == 0)) {
    // Selection is somewhere inside the text node; the offset is aNodeOffset
    *aResult = aNodeOffset;
  } else {
    // Selection is on the mozBR node, so offset should be set to the length
    // of the text node.
    if (textNode) {
      rv = textNode->GetLength(&length);
      NS_ENSURE_SUCCESS(rv, rv);
      *aResult = PRInt32(length);
    }
  }

  return NS_OK;
}

nsresult
nsTextControlFrame::OffsetToDOMPoint(PRInt32 aOffset,
                                     nsIDOMNode** aResult,
                                     PRInt32* aPosition)
{
  NS_ENSURE_ARG_POINTER(aResult && aPosition);

  *aResult = nsnull;
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

  PRUint32 length = 0;

  rv = nodeList->GetLength(&length);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(length <= 2, "We should have one text node and one mozBR at most");

  nsCOMPtr<nsIDOMNode> firstNode;
  rv = nodeList->Item(0, getter_AddRefs(firstNode));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(firstNode);

  if (length == 0 || aOffset < 0) {
    NS_IF_ADDREF(*aResult = rootNode);
    *aPosition = 0;
  } else if (textNode) {
    NS_IF_ADDREF(*aResult = firstNode);
    *aPosition = aOffset;
  } else {
    NS_IF_ADDREF(*aResult = rootNode);
    *aPosition = 0;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTextControlFrame::GetSelectionRange(PRInt32* aSelectionStart, PRInt32* aSelectionEnd)
{
  // make sure we have an editor
  nsresult rv = EnsureEditorInitialized();
  NS_ENSURE_SUCCESS(rv, rv);

  *aSelectionStart = 0;
  *aSelectionEnd = 0;

  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element");
  nsISelectionController* selCon = txtCtrl->GetSelectionController();
  NS_ENSURE_TRUE(selCon, NS_ERROR_FAILURE);
  nsCOMPtr<nsISelection> selection;
  rv = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));  
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);

  PRInt32 numRanges = 0;
  selection->GetRangeCount(&numRanges);

  if (numRanges < 1)
    return NS_OK;

  // We only operate on the first range in the selection!

  nsCOMPtr<nsIDOMRange> firstRange;
  rv = selection->GetRangeAt(0, getter_AddRefs(firstRange));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(firstRange, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMNode> startNode, endNode;
  PRInt32 startOffset = 0, endOffset = 0;

  // Get the start point of the range.

  rv = firstRange->GetStartContainer(getter_AddRefs(startNode));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(startNode, NS_ERROR_FAILURE);

  rv = firstRange->GetStartOffset(&startOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the end point of the range.

  rv = firstRange->GetEndContainer(getter_AddRefs(endNode));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(endNode, NS_ERROR_FAILURE);

  rv = firstRange->GetEndOffset(&endOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  // Convert the start point to a selection offset.

  rv = DOMPointToOffset(startNode, startOffset, aSelectionStart);
  NS_ENSURE_SUCCESS(rv, rv);

  // Convert the end point to a selection offset.

  return DOMPointToOffset(endNode, endOffset, aSelectionEnd);
}

/////END INTERFACE IMPLEMENTATIONS

////NSIFRAME
NS_IMETHODIMP
nsTextControlFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                     nsIAtom*        aAttribute,
                                     PRInt32         aModType)
{
  // First, check for the placeholder attribute, because it doesn't
  // depend on the editor being present.
  if (nsGkAtoms::placeholder == aAttribute)
  {
    nsWeakFrame weakFrame(this);
    nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
    NS_ASSERTION(txtCtrl, "Content not a text control element");
    txtCtrl->UpdatePlaceholderText(PR_TRUE);
    NS_ENSURE_STATE(weakFrame.IsAlive());
  }

  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element");
  nsISelectionController* selCon = txtCtrl->GetSelectionController();
  const PRBool needEditor = nsGkAtoms::maxlength == aAttribute ||
                            nsGkAtoms::readonly == aAttribute ||
                            nsGkAtoms::disabled == aAttribute ||
                            nsGkAtoms::spellcheck == aAttribute;
  nsCOMPtr<nsIEditor> editor;
  if (needEditor) {
    GetEditor(getter_AddRefs(editor));
  }
  if ((needEditor && !editor) || !selCon)
    return nsBoxFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);;

  nsresult rv = NS_OK;

  if (nsGkAtoms::maxlength == aAttribute) 
  {
    PRInt32 maxLength;
    PRBool maxDefined = GetMaxLength(&maxLength);
    
    nsCOMPtr<nsIPlaintextEditor> textEditor = do_QueryInterface(editor);
    if (textEditor)
    {
      if (maxDefined) 
      {  // set the maxLength attribute
          textEditor->SetMaxTextLength(maxLength);
        // if maxLength>docLength, we need to truncate the doc content
      }
      else { // unset the maxLength attribute
          textEditor->SetMaxTextLength(-1);
      }
    }
    rv = NS_OK; // don't propagate the error
  } 
  else if (nsGkAtoms::readonly == aAttribute) 
  {
    PRUint32 flags;
    editor->GetFlags(&flags);
    if (AttributeExists(nsGkAtoms::readonly))
    { // set readonly
      flags |= nsIPlaintextEditor::eEditorReadonlyMask;
      if (nsContentUtils::IsFocusedContent(mContent))
        selCon->SetCaretEnabled(PR_FALSE);
    }
    else 
    { // unset readonly
      flags &= ~(nsIPlaintextEditor::eEditorReadonlyMask);
      if (!(flags & nsIPlaintextEditor::eEditorDisabledMask) &&
          nsContentUtils::IsFocusedContent(mContent))
        selCon->SetCaretEnabled(PR_TRUE);
    }
    editor->SetFlags(flags);
  }
  else if (nsGkAtoms::disabled == aAttribute) 
  {
    PRUint32 flags;
    editor->GetFlags(&flags);
    if (AttributeExists(nsGkAtoms::disabled))
    { // set disabled
      flags |= nsIPlaintextEditor::eEditorDisabledMask;
      selCon->SetDisplaySelection(nsISelectionController::SELECTION_OFF);
      if (nsContentUtils::IsFocusedContent(mContent))
        selCon->SetCaretEnabled(PR_FALSE);
    }
    else 
    { // unset disabled
      flags &= ~(nsIPlaintextEditor::eEditorDisabledMask);
      selCon->SetDisplaySelection(nsISelectionController::SELECTION_HIDDEN);
      if (nsContentUtils::IsFocusedContent(mContent)) {
        selCon->SetCaretEnabled(PR_TRUE);
      }
    }
    editor->SetFlags(flags);
  }
  else if (!mUseEditor && nsGkAtoms::value == aAttribute) {
    UpdateValueDisplay(PR_TRUE);
  }
  // Allow the base class to handle common attributes supported
  // by all form elements... 
  else {
    rv = nsBoxFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
  }

  return rv;
}


nsresult
nsTextControlFrame::GetText(nsString& aText)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element");
  if (IsSingleLineTextControl()) {
    // There will be no line breaks so we can ignore the wrap property.
    txtCtrl->GetTextEditorValue(aText, PR_TRUE);
  } else {
    nsCOMPtr<nsIDOMHTMLTextAreaElement> textArea = do_QueryInterface(mContent);
    if (textArea) {
      rv = textArea->GetValue(aText);
    }
  }
  return rv;
}


nsresult
nsTextControlFrame::GetPhonetic(nsAString& aPhonetic)
{
  aPhonetic.Truncate(0); 

  nsCOMPtr<nsIEditor> editor;
  nsresult rv = GetEditor(getter_AddRefs(editor));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIEditorIMESupport> imeSupport = do_QueryInterface(editor);
  if (imeSupport) {
    nsCOMPtr<nsIPhonetic> phonetic = do_QueryInterface(imeSupport);
    if (phonetic)
      phonetic->GetPhonetic(aPhonetic);
  }
  return NS_OK;
}

///END NSIFRAME OVERLOADS
/////BEGIN PROTECTED METHODS

PRBool
nsTextControlFrame::GetMaxLength(PRInt32* aSize)
{
  *aSize = -1;

  nsGenericHTMLElement *content = nsGenericHTMLElement::FromContent(mContent);
  if (content) {
    const nsAttrValue* attr = content->GetParsedAttr(nsGkAtoms::maxlength);
    if (attr && attr->Type() == nsAttrValue::eInteger) {
      *aSize = attr->GetIntegerValue();

      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

// this is where we propagate a content changed event
void
nsTextControlFrame::FireOnInput()
{
  if (!mNotifyOnInput)
    return; // if notification is turned off, do nothing
  
  // Dispatch the "input" event
  nsEventStatus status = nsEventStatus_eIgnore;
  nsUIEvent event(PR_TRUE, NS_FORM_INPUT, 0);

  // Have the content handle the event, propagating it according to normal
  // DOM rules.
  nsCOMPtr<nsIPresShell> shell = PresContext()->PresShell();
  shell->HandleEventWithTarget(&event, nsnull, mContent, &status);
}

nsresult
nsTextControlFrame::InitFocusedValue()
{
  return GetText(mFocusedValue);
}

NS_IMETHODIMP
nsTextControlFrame::CheckFireOnChange()
{
  nsString value;
  GetText(value);
  if (!mFocusedValue.Equals(value))
  {
    mFocusedValue = value;
    // Dispatch the change event
    nsEventStatus status = nsEventStatus_eIgnore;
    nsInputEvent event(PR_TRUE, NS_FORM_CHANGE, nsnull);
    nsCOMPtr<nsIPresShell> shell = PresContext()->PresShell();
    shell->HandleEventWithTarget(&event, nsnull, mContent, &status);
  }
  return NS_OK;
}

// END IMPLEMENTING NS_IFORMCONTROLFRAME

NS_IMETHODIMP
nsTextControlFrame::SetInitialChildList(nsIAtom*        aListName,
                                        nsFrameList&    aChildList)
{
  nsresult rv = nsBoxFrame::SetInitialChildList(aListName, aChildList);

  nsIFrame* first = GetFirstChild(nsnull);

  // Mark the scroll frame as being a reflow root. This will allow
  // incremental reflows to be initiated at the scroll frame, rather
  // than descending from the root frame of the frame hierarchy.
  if (first) {
    first->AddStateBits(NS_FRAME_REFLOW_ROOT);
  }

  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element");
  txtCtrl->InitializeKeyboardEventListeners();
  return rv;
}

PRBool
nsTextControlFrame::IsScrollable() const
{
  return !IsSingleLineTextControl();
}

void
nsTextControlFrame::SetValueChanged(PRBool aValueChanged)
{
  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element");
  txtCtrl->SetValueChanged(aValueChanged);
}


nsresult
nsTextControlFrame::UpdateValueDisplay(PRBool aNotify,
                                       PRBool aBeforeEditorInit,
                                       const nsAString *aValue)
{
  if (!IsSingleLineTextControl()) // textareas don't use this
    return NS_OK;

  nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent());
  NS_ASSERTION(txtCtrl, "Content not a text control element");
  nsIContent* rootNode = txtCtrl->GetRootEditorNode();

  NS_PRECONDITION(rootNode, "Must have a div content\n");
  NS_PRECONDITION(!mUseEditor,
                  "Do not call this after editor has been initialized");
  NS_ASSERTION(txtCtrl->GetPlaceholderNode(), "A placeholder div must exist");

  nsIContent *textContent = rootNode->GetChildAt(0);
  if (!textContent) {
    // Set up a textnode with our value
    nsCOMPtr<nsIContent> textNode;
    nsresult rv = NS_NewTextNode(getter_AddRefs(textNode),
                                 mContent->NodeInfo()->NodeInfoManager());
    NS_ENSURE_SUCCESS(rv, rv);

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
    txtCtrl->GetTextEditorValue(value, PR_TRUE);
  }

  // Update the display of the placeholder value if needed.
  // We don't need to do this if we're about to initialize the
  // editor, since EnsureEditorInitialized takes care of this.
  if (!aBeforeEditorInit)
  {
    nsWeakFrame weakFrame(this);
    txtCtrl->SetPlaceholderClass(value.IsEmpty(), aNotify);
    NS_ENSURE_STATE(weakFrame.IsAlive());
  }

  if (aBeforeEditorInit && value.IsEmpty()) {
    rootNode->RemoveChildAt(0, PR_TRUE, PR_FALSE);
    return NS_OK;
  }

  if (!value.IsEmpty() && IsPasswordTextControl()) {
    nsTextEditRules::FillBufWithPWChars(&value, value.Length());
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

