/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "nsIEditor.h"

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
    return do_QueryFrame(GetFirstPrincipalChild());
  }

  virtual nscoord GetMinWidth(nsRenderingContext* aRenderingContext);
  virtual nsSize ComputeAutoSize(nsRenderingContext *aRenderingContext,
                                 nsSize aCBSize, nscoord aAvailableWidth,
                                 nsSize aMargin, nsSize aBorder,
                                 nsSize aPadding, bool aShrinkWrap);

  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  virtual nsSize GetPrefSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetMinSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetMaxSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nscoord GetBoxAscent(nsBoxLayoutState& aBoxLayoutState);
  virtual bool IsCollapsed();

  DECL_DO_GLOBAL_REFLOW_COUNT_DSP(nsTextControlFrame, nsStackFrame)

  virtual bool IsLeaf() const;
  
#ifdef ACCESSIBILITY
  virtual already_AddRefed<Accessible> CreateAccessible();
#endif

#ifdef NS_DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    aResult.AssignLiteral("nsTextControlFrame");
    return NS_OK;
  }
#endif

  virtual bool IsFrameOfType(PRUint32 aFlags) const
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

  NS_IMETHOD SetInitialChildList(ChildListID     aListID,
                                 nsFrameList&    aChildList);

//==== BEGIN NSIFORMCONTROLFRAME
  virtual void SetFocus(bool aOn , bool aRepaint); 
  virtual nsresult SetFormProperty(nsIAtom* aName, const nsAString& aValue);
  virtual nsresult GetFormProperty(nsIAtom* aName, nsAString& aValue) const; 


//==== END NSIFORMCONTROLFRAME

//==== NSITEXTCONTROLFRAME

  NS_IMETHOD    GetEditor(nsIEditor **aEditor);
  NS_IMETHOD    GetTextLength(PRInt32* aTextLength);
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
  void SetValueChanged(bool aValueChanged);
  
  // called by the focus listener
  nsresult MaybeBeginSecureKeyboardInput();
  void MaybeEndSecureKeyboardInput();

  NS_STACK_CLASS class ValueSetter {
  public:
    ValueSetter(nsTextControlFrame* aFrame,
                nsIEditor* aEditor)
      : mFrame(aFrame)
      , mEditor(aEditor)
      , mCanceled(false)
    {
      MOZ_ASSERT(aFrame);
      MOZ_ASSERT(aEditor);

      // To protect against a reentrant call to SetValue, we check whether
      // another SetValue is already happening for this frame.  If it is,
      // we must wait until we unwind to re-enable oninput events.
      mEditor->GetSuppressDispatchingInputEvent(&mOuterTransaction);
    }
    void Cancel() {
      mCanceled = true;
    }
    void Init() {
      mEditor->SetSuppressDispatchingInputEvent(true);
    }
    ~ValueSetter() {
      mEditor->SetSuppressDispatchingInputEvent(mOuterTransaction);

      if (mCanceled) {
        return;
      }
    }

  private:
    nsTextControlFrame* mFrame;
    nsCOMPtr<nsIEditor> mEditor;
    bool mOuterTransaction;
    bool mCanceled;
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

  DEFINE_TEXTCTRL_CONST_FORWARDER(bool, IsSingleLineTextControl)
  DEFINE_TEXTCTRL_CONST_FORWARDER(bool, IsTextArea)
  DEFINE_TEXTCTRL_CONST_FORWARDER(bool, IsPlainTextControl)
  DEFINE_TEXTCTRL_CONST_FORWARDER(bool, IsPasswordTextControl)
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
        bool observes = shell->ObservesNativeAnonMutationsForPrint();
        shell->ObserveNativeAnonMutationsForPrint(true);
        // This can cause the frame to be destroyed (and call Revoke())
        mFrame->EnsureEditorInitialized();
        shell->ObserveNativeAnonMutationsForPrint(observes);

        // The frame can *still* be destroyed even though we have a scriptblocker
        // Bug 682684
        if (!mFrame)
          return NS_ERROR_FAILURE;

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
  bool IsScrollable() const;

  /**
   * Update the textnode under our anonymous div to show the new
   * value. This should only be called when we have no editor yet.
   * @throws NS_ERROR_UNEXPECTED if the div has no text content
   */
  nsresult UpdateValueDisplay(bool aNotify,
                              bool aBeforeEditorInit = false,
                              const nsAString *aValue = nsnull);

  /**
   * Get the maxlength attribute
   * @param aMaxLength the value of the max length attr
   * @returns false if attr not defined
   */
  bool GetMaxLength(PRInt32* aMaxLength);

  /**
   * Find out whether an attribute exists on the content or not.
   * @param aAtt the attribute to determine the existence of
   * @returns false if it does not exist
   */
  bool AttributeExists(nsIAtom *aAtt) const
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
                             nsSize&             aIntrinsicSize,
                             float               aFontSizeInflation);

  nsresult ScrollSelectionIntoView();

private:
  //helper methods
  nsresult SetSelectionInternal(nsIDOMNode *aStartNode, PRInt32 aStartOffset,
                                nsIDOMNode *aEndNode, PRInt32 aEndOffset,
                                SelectionDirection aDirection = eNone);
  nsresult SelectAllOrCollapseToEndOfText(bool aSelect);
  nsresult SetSelectionEndPoints(PRInt32 aSelStart, PRInt32 aSelEnd,
                                 SelectionDirection aDirection = eNone);

  /**
   * Return the root DOM element, and implicitly initialize the editor if needed.
   */
  nsresult GetRootNodeAndInitializeEditor(nsIDOMElement **aRootElement);

  void FinishedInitializer() {
    Properties().Delete(TextControlInitializer());
  }

private:
  // these packed bools could instead use the high order bits on mState, saving 4 bytes 
  bool mUseEditor;
  bool mIsProcessing;
  // Keep track if we have asked a placeholder node creation.
  bool mUsePlaceholder;

#ifdef DEBUG
  bool mInEditorInitialization;
  friend class EditorInitializerEntryTracker;
#endif

  nsRevocableEventPtr<ScrollOnFocusEvent> mScrollEvent;
};

#endif


