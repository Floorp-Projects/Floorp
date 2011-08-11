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

#ifndef nsTextControlFrame_h___
#define nsTextControlFrame_h___

#include "nsStackFrame.h"
#include "nsBlockFrame.h"
#include "nsIFormControlFrame.h"
#include "nsIAnonymousContentCreator.h"
#include "nsITextControlFrame.h"
#include "nsDisplayList.h"
#include "nsIScrollableFrame.h"
#include "nsStubMutationObserver.h"
#include "nsITextControlElement.h"
#include "nsIStatefulFrame.h"
#include "nsContentUtils.h" // nsAutoScriptBlocker

class nsIEditor;
class nsISelectionController;
class nsIDOMCharacterData;
#ifdef ACCESSIBILITY
class nsIAccessible;
#endif
class EditorInitializerEntryTracker;
class nsTextEditorState;

class nsTextControlFrame : public nsStackFrame,
                           public nsIAnonymousContentCreator,
                           public nsITextControlFrame,
                           public nsIStatefulFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  NS_DECLARE_FRAME_PROPERTY(ContentScrollPos, DestroyPoint)

  nsTextControlFrame(nsIPresShell* aShell, nsStyleContext* aContext);
  virtual ~nsTextControlFrame();

  virtual void DestroyFrom(nsIFrame* aDestructRoot);

  virtual nsIScrollableFrame* GetScrollTargetFrame() {
    if (!IsScrollable())
      return nsnull;
    return do_QueryFrame(GetFirstChild(nsnull));
  }

  virtual nscoord GetMinWidth(nsRenderingContext* aRenderingContext);
  virtual nsSize ComputeAutoSize(nsRenderingContext *aRenderingContext,
                                 nsSize aCBSize, nscoord aAvailableWidth,
                                 nsSize aMargin, nsSize aBorder,
                                 nsSize aPadding, PRBool aShrinkWrap);

  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  virtual nsSize GetPrefSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetMinSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetMaxSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nscoord GetBoxAscent(nsBoxLayoutState& aBoxLayoutState);
  virtual PRBool IsCollapsed(nsBoxLayoutState& aBoxLayoutState);

  DECL_DO_GLOBAL_REFLOW_COUNT_DSP(nsTextControlFrame, nsStackFrame)

  virtual PRBool IsLeaf() const;
  
#ifdef ACCESSIBILITY
  virtual already_AddRefed<nsAccessible> CreateAccessible();
#endif

#ifdef NS_DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    aResult.AssignLiteral("nsTextControlFrame");
    return NS_OK;
  }
#endif

  virtual PRBool IsFrameOfType(PRUint32 aFlags) const
  {
    // nsStackFrame is already both of these, but that's somewhat bogus,
    // and we really mean it.
    return nsStackFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements);
  virtual void AppendAnonymousContentTo(nsBaseContentList& aElements,
                                        PRUint32 aFilter);

  // Utility methods to set current widget state

  NS_IMETHOD SetInitialChildList(nsIAtom*        aListName,
                                 nsFrameList&    aChildList);

//==== BEGIN NSIFORMCONTROLFRAME
  virtual void SetFocus(PRBool aOn , PRBool aRepaint); 
  virtual nsresult SetFormProperty(nsIAtom* aName, const nsAString& aValue);
  virtual nsresult GetFormProperty(nsIAtom* aName, nsAString& aValue) const; 


//==== END NSIFORMCONTROLFRAME

//==== NSITEXTCONTROLFRAME

  NS_IMETHOD    GetEditor(nsIEditor **aEditor);
  NS_IMETHOD    GetTextLength(PRInt32* aTextLength);
  NS_IMETHOD    CheckFireOnChange();
  NS_IMETHOD    SetSelectionStart(PRInt32 aSelectionStart);
  NS_IMETHOD    SetSelectionEnd(PRInt32 aSelectionEnd);
  NS_IMETHOD    SetSelectionRange(PRInt32 aSelectionStart,
                                  PRInt32 aSelectionEnd,
                                  SelectionDirection aDirection = eNone);
  NS_IMETHOD    GetSelectionRange(PRInt32* aSelectionStart,
                                  PRInt32* aSelectionEnd,
                                  SelectionDirection* aDirection = nsnull);
  NS_IMETHOD    GetOwnedSelectionController(nsISelectionController** aSelCon);
  virtual nsFrameSelection* GetOwnedFrameSelection();

  nsresult GetPhonetic(nsAString& aPhonetic);

  /**
   * Ensure mEditor is initialized with the proper flags and the default value.
   * @throws NS_ERROR_NOT_INITIALIZED if mEditor has not been created
   * @throws various and sundry other things
   */
  virtual nsresult EnsureEditorInitialized();

//==== END NSITEXTCONTROLFRAME

//==== NSISTATEFULFRAME

  NS_IMETHOD SaveState(SpecialStateID aStateID, nsPresState** aState);
  NS_IMETHOD RestoreState(nsPresState* aState);

//=== END NSISTATEFULFRAME

//==== OVERLOAD of nsIFrame
  virtual nsIAtom* GetType() const;

  /** handler for attribute changes to mContent */
  NS_IMETHOD AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);

  nsresult GetText(nsString& aText);

  NS_DECL_QUERYFRAME

  // Temp reference to scriptrunner
  // We could make these auto-Revoking via the "delete" entry for safety
  NS_DECLARE_FRAME_PROPERTY(TextControlInitializer, nsnull)


public: //for methods who access nsTextControlFrame directly
  void FireOnInput(PRBool aTrusted);
  void SetValueChanged(PRBool aValueChanged);
  /** Called when the frame is focused, to remember the value for onChange. */
  nsresult InitFocusedValue();

  void SetFireChangeEventState(PRBool aNewState)
  {
    mFireChangeEventState = aNewState;
  }

  PRBool GetFireChangeEventState() const
  {
    return mFireChangeEventState;
  }    

  // called by the focus listener
  nsresult MaybeBeginSecureKeyboardInput();
  void MaybeEndSecureKeyboardInput();

  class ValueSetter {
  public:
    ValueSetter(nsTextControlFrame* aFrame,
                PRBool aHasFocusValue)
      : mFrame(aFrame)
      // This method isn't used for user-generated changes, except for calls
      // from nsFileControlFrame which sets mFireChangeEventState==true and
      // restores it afterwards (ie. we want 'change' events for those changes).
      // Focused value must be updated to prevent incorrect 'change' events,
      // but only if user hasn't changed the value.
      , mFocusValueInit(!mFrame->mFireChangeEventState && aHasFocusValue)
      , mOuterTransaction(false)
      , mInited(false)
    {
      NS_ASSERTION(aFrame, "Should pass a valid frame");
    }
    void Cancel() {
      mInited = PR_FALSE;
    }
    void Init() {
      // Since this code does not handle user-generated changes to the text,
      // make sure we don't fire oninput when the editor notifies us.
      // (mNotifyOnInput must be reset before we return).

      // To protect against a reentrant call to SetValue, we check whether
      // another SetValue is already happening for this frame.  If it is,
      // we must wait until we unwind to re-enable oninput events.
      mOuterTransaction = mFrame->mNotifyOnInput;
      if (mOuterTransaction)
        mFrame->mNotifyOnInput = PR_FALSE;

      mInited = PR_TRUE;
    }
    ~ValueSetter() {
      if (!mInited)
        return;

      if (mOuterTransaction)
        mFrame->mNotifyOnInput = PR_TRUE;

      if (mFocusValueInit) {
        // Reset mFocusedValue so the onchange event doesn't fire incorrectly.
        mFrame->InitFocusedValue();
      }
    }

  private:
    nsTextControlFrame* mFrame;
    PRPackedBool mFocusValueInit;
    PRPackedBool mOuterTransaction;
    PRPackedBool mInited;
  };
  friend class ValueSetter;

#define DEFINE_TEXTCTRL_FORWARDER(type, name)                                  \
  type name() {                                                                \
    nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent()); \
    NS_ASSERTION(txtCtrl, "Content not a text control element");               \
    return txtCtrl->name();                                                    \
  }
#define DEFINE_TEXTCTRL_CONST_FORWARDER(type, name)                            \
  type name() const {                                                          \
    nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent()); \
    NS_ASSERTION(txtCtrl, "Content not a text control element");               \
    return txtCtrl->name();                                                    \
  }

  DEFINE_TEXTCTRL_CONST_FORWARDER(PRBool, IsSingleLineTextControl)
  DEFINE_TEXTCTRL_CONST_FORWARDER(PRBool, IsTextArea)
  DEFINE_TEXTCTRL_CONST_FORWARDER(PRBool, IsPlainTextControl)
  DEFINE_TEXTCTRL_CONST_FORWARDER(PRBool, IsPasswordTextControl)
  DEFINE_TEXTCTRL_FORWARDER(PRInt32, GetCols)
  DEFINE_TEXTCTRL_FORWARDER(PRInt32, GetWrapCols)
  DEFINE_TEXTCTRL_FORWARDER(PRInt32, GetRows)

#undef DEFINE_TEXTCTRL_CONST_FORWARDER
#undef DEFINE_TEXTCTRL_FORWARDER

protected:
  class EditorInitializer;
  friend class EditorInitializer;
  friend class nsTextEditorState; // needs access to UpdateValueDisplay

  class EditorInitializer : public nsRunnable {
  public:
    EditorInitializer(nsTextControlFrame* aFrame) :
      mFrame(aFrame) {}

    NS_IMETHOD Run() {
      if (mFrame) {
        // need to block script to avoid bug 669767
        nsAutoScriptBlocker scriptBlocker;

        nsCOMPtr<nsIPresShell> shell =
          mFrame->PresContext()->GetPresShell();
        PRBool observes = shell->ObservesNativeAnonMutationsForPrint();
        shell->ObserveNativeAnonMutationsForPrint(PR_TRUE);
        // This can cause the frame to be destroyed (and call Revoke()
        mFrame->EnsureEditorInitialized();
        shell->ObserveNativeAnonMutationsForPrint(observes);

        NS_ASSERTION(mFrame,"Frame destroyed even though we had a scriptblocker");
        mFrame->FinishedInitializer();
      }
      return NS_OK;
    }

    // avoids use of nsWeakFrame
    void Revoke() {
      mFrame = nsnull;
    }

  private:
    nsTextControlFrame* mFrame;
  };

  class ScrollOnFocusEvent;
  friend class ScrollOnFocusEvent;

  class ScrollOnFocusEvent : public nsRunnable {
  public:
    ScrollOnFocusEvent(nsTextControlFrame* aFrame) :
      mFrame(aFrame) {}

    NS_DECL_NSIRUNNABLE

    void Revoke() {
      mFrame = nsnull;
    }

  private:
    nsTextControlFrame* mFrame;
  };

  nsresult DOMPointToOffset(nsIDOMNode* aNode, PRInt32 aNodeOffset, PRInt32 *aResult);
  nsresult OffsetToDOMPoint(PRInt32 aOffset, nsIDOMNode** aResult, PRInt32* aPosition);

  /**
   * Find out whether this control is scrollable (i.e. if it is not a single
   * line text control)
   * @return whether this control is scrollable
   */
  PRBool IsScrollable() const;

  /**
   * Update the textnode under our anonymous div to show the new
   * value. This should only be called when we have no editor yet.
   * @throws NS_ERROR_UNEXPECTED if the div has no text content
   */
  nsresult UpdateValueDisplay(PRBool aNotify,
                              PRBool aBeforeEditorInit = PR_FALSE,
                              const nsAString *aValue = nsnull);

  /**
   * Get the maxlength attribute
   * @param aMaxLength the value of the max length attr
   * @returns PR_FALSE if attr not defined
   */
  PRBool GetMaxLength(PRInt32* aMaxLength);

  /**
   * Find out whether an attribute exists on the content or not.
   * @param aAtt the attribute to determine the existence of
   * @returns PR_FALSE if it does not exist
   */
  PRBool AttributeExists(nsIAtom *aAtt) const
  { return mContent && mContent->HasAttr(kNameSpaceID_None, aAtt); }

  /**
   * We call this when we are being destroyed or removed from the PFM.
   * @param aPresContext the current pres context
   */
  void PreDestroy();

  // Compute our intrinsic size.  This does not include any borders, paddings,
  // etc.  Just the size of our actual area for the text (and the scrollbars,
  // for <textarea>).
  nsresult CalcIntrinsicSize(nsRenderingContext* aRenderingContext,
                             nsSize&              aIntrinsicSize);

  nsresult ScrollSelectionIntoView();

private:
  //helper methods
  nsresult SetSelectionInternal(nsIDOMNode *aStartNode, PRInt32 aStartOffset,
                                nsIDOMNode *aEndNode, PRInt32 aEndOffset,
                                SelectionDirection aDirection = eNone);
  nsresult SelectAllOrCollapseToEndOfText(PRBool aSelect);
  nsresult SetSelectionEndPoints(PRInt32 aSelStart, PRInt32 aSelEnd,
                                 SelectionDirection aDirection = eNone);

  // accessors for the notify on input flag
  PRBool GetNotifyOnInput() const { return mNotifyOnInput; }
  void SetNotifyOnInput(PRBool val) { mNotifyOnInput = val; }

  /**
   * Return the root DOM element, and implicitly initialize the editor if needed.
   */
  nsresult GetRootNodeAndInitializeEditor(nsIDOMElement **aRootElement);

  void FinishedInitializer() {
    Properties().Delete(TextControlInitializer());
  }

private:
  // these packed bools could instead use the high order bits on mState, saving 4 bytes 
  PRPackedBool mUseEditor;
  PRPackedBool mIsProcessing;
  PRPackedBool mNotifyOnInput;//default this to off to stop any notifications until setup is complete
  // Calls to SetValue will be treated as user values (i.e. trigger onChange
  // eventually) when mFireChangeEventState==true, this is used by nsFileControlFrame.
  PRPackedBool mFireChangeEventState;
  // Keep track if we have asked a placeholder node creation.
  PRPackedBool mUsePlaceholder;

#ifdef DEBUG
  PRPackedBool mInEditorInitialization;
  friend class EditorInitializerEntryTracker;
#endif

  nsString mFocusedValue;
  nsRevocableEventPtr<ScrollOnFocusEvent> mScrollEvent;
};

#endif


