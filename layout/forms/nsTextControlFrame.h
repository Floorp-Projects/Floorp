/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTextControlFrame_h___
#define nsTextControlFrame_h___

#include "mozilla/Attributes.h"
#include "nsContainerFrame.h"
#include "nsIAnonymousContentCreator.h"
#include "nsIContent.h"
#include "nsITextControlFrame.h"
#include "nsITextControlElement.h"
#include "nsIStatefulFrame.h"

class nsISelectionController;
class EditorInitializerEntryTracker;
class nsTextEditorState;
class nsIEditor;
namespace mozilla {
enum class CSSPseudoElementType : uint8_t;
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla

class nsTextControlFrame final : public nsContainerFrame,
                                 public nsIAnonymousContentCreator,
                                 public nsITextControlFrame,
                                 public nsIStatefulFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  NS_DECLARE_FRAME_PROPERTY_DELETABLE(ContentScrollPos, nsPoint)

  explicit nsTextControlFrame(nsStyleContext* aContext);
  virtual ~nsTextControlFrame();

  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;

  virtual nsIScrollableFrame* GetScrollTargetFrame() override {
    return do_QueryFrame(PrincipalChildList().FirstChild());
  }

  virtual nscoord GetMinISize(nsRenderingContext* aRenderingContext) override;
  virtual nscoord GetPrefISize(nsRenderingContext* aRenderingContext) override;

  virtual mozilla::LogicalSize
  ComputeAutoSize(nsRenderingContext*         aRenderingContext,
                  mozilla::WritingMode        aWM,
                  const mozilla::LogicalSize& aCBSize,
                  nscoord                     aAvailableISize,
                  const mozilla::LogicalSize& aMargin,
                  const mozilla::LogicalSize& aBorder,
                  const mozilla::LogicalSize& aPadding,
                  ComputeSizeFlags            aFlags) override;

  virtual void Reflow(nsPresContext*           aPresContext,
                      ReflowOutput&     aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus&          aStatus) override;

  virtual nsSize GetXULMinSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual bool IsXULCollapsed() override;

  virtual bool IsLeaf() const override;
  
#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() override;
#endif

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override
  {
    aResult.AssignLiteral("nsTextControlFrame");
    return NS_OK;
  }
#endif

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    // nsStackFrame is already both of these, but that's somewhat bogus,
    // and we really mean it.
    return nsContainerFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) override;
  virtual void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                        uint32_t aFilter) override;

  virtual void SetInitialChildList(ChildListID     aListID,
                                   nsFrameList&    aChildList) override;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  virtual mozilla::dom::Element*
  GetPseudoElement(mozilla::CSSPseudoElementType aType) override;

//==== BEGIN NSIFORMCONTROLFRAME
  virtual void SetFocus(bool aOn , bool aRepaint) override;
  virtual nsresult SetFormProperty(nsIAtom* aName, const nsAString& aValue) override;

//==== END NSIFORMCONTROLFRAME

//==== NSITEXTCONTROLFRAME

  NS_IMETHOD    GetEditor(nsIEditor **aEditor) override;
  NS_IMETHOD    SetSelectionStart(int32_t aSelectionStart) override;
  NS_IMETHOD    SetSelectionEnd(int32_t aSelectionEnd) override;
  NS_IMETHOD    SetSelectionRange(int32_t aSelectionStart,
                                  int32_t aSelectionEnd,
                                  SelectionDirection aDirection = eNone) override;
  NS_IMETHOD    GetSelectionRange(int32_t* aSelectionStart,
                                  int32_t* aSelectionEnd,
                                  SelectionDirection* aDirection = nullptr) override;
  NS_IMETHOD    GetOwnedSelectionController(nsISelectionController** aSelCon) override;
  virtual nsFrameSelection* GetOwnedFrameSelection() override;

  nsresult GetPhonetic(nsAString& aPhonetic) override;

  /**
   * Ensure mEditor is initialized with the proper flags and the default value.
   * @throws NS_ERROR_NOT_INITIALIZED if mEditor has not been created
   * @throws various and sundry other things
   */
  virtual nsresult EnsureEditorInitialized() override;

//==== END NSITEXTCONTROLFRAME

//==== NSISTATEFULFRAME

  NS_IMETHOD SaveState(nsPresState** aState) override;
  NS_IMETHOD RestoreState(nsPresState* aState) override;

//=== END NSISTATEFULFRAME

//==== OVERLOAD of nsIFrame
  virtual nsIAtom* GetType() const override;

  /** handler for attribute changes to mContent */
  virtual nsresult AttributeChanged(int32_t         aNameSpaceID,
                                    nsIAtom*        aAttribute,
                                    int32_t         aModType) override;

  nsresult GetText(nsString& aText);

  virtual nsresult PeekOffset(nsPeekOffsetStruct *aPos) override;

  NS_DECL_QUERYFRAME

protected:
  /**
   * Launch the reflow on the child frames - see nsTextControlFrame::Reflow()
   */
  void ReflowTextControlChild(nsIFrame*                aFrame,
                              nsPresContext*           aPresContext,
                              const ReflowInput& aReflowInput,
                              nsReflowStatus&          aStatus,
                              ReflowOutput& aParentDesiredSize);

public: //for methods who access nsTextControlFrame directly
  void SetValueChanged(bool aValueChanged);
  
  // called by the focus listener
  nsresult MaybeBeginSecureKeyboardInput();
  void MaybeEndSecureKeyboardInput();

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
  DEFINE_TEXTCTRL_CONST_FORWARDER(int32_t, GetCols)
  DEFINE_TEXTCTRL_CONST_FORWARDER(int32_t, GetWrapCols)
  DEFINE_TEXTCTRL_CONST_FORWARDER(int32_t, GetRows)

#undef DEFINE_TEXTCTRL_CONST_FORWARDER
#undef DEFINE_TEXTCTRL_FORWARDER

protected:
  class EditorInitializer;
  friend class EditorInitializer;
  friend class nsTextEditorState; // needs access to UpdateValueDisplay

  // Temp reference to scriptrunner
  // We could make these auto-Revoking via the "delete" entry for safety
  NS_DECLARE_FRAME_PROPERTY_WITHOUT_DTOR(TextControlInitializer,
                                         EditorInitializer)

  class EditorInitializer : public mozilla::Runnable {
  public:
    explicit EditorInitializer(nsTextControlFrame* aFrame) :
      mFrame(aFrame) {}

    NS_IMETHOD Run() override;

    // avoids use of nsWeakFrame
    void Revoke() {
      mFrame = nullptr;
    }

  private:
    nsTextControlFrame* mFrame;
  };

  class ScrollOnFocusEvent;
  friend class ScrollOnFocusEvent;

  class ScrollOnFocusEvent : public mozilla::Runnable {
  public:
    explicit ScrollOnFocusEvent(nsTextControlFrame* aFrame) :
      mFrame(aFrame) {}

    NS_DECL_NSIRUNNABLE

    void Revoke() {
      mFrame = nullptr;
    }

  private:
    nsTextControlFrame* mFrame;
  };

  nsresult OffsetToDOMPoint(int32_t aOffset, nsIDOMNode** aResult, int32_t* aPosition);

  /**
   * Update the textnode under our anonymous div to show the new
   * value. This should only be called when we have no editor yet.
   * @throws NS_ERROR_UNEXPECTED if the div has no text content
   */
  nsresult UpdateValueDisplay(bool aNotify,
                              bool aBeforeEditorInit = false,
                              const nsAString *aValue = nullptr);

  /**
   * Get the maxlength attribute
   * @param aMaxLength the value of the max length attr
   * @returns false if attr not defined
   */
  bool GetMaxLength(int32_t* aMaxLength);

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
  mozilla::LogicalSize CalcIntrinsicSize(nsRenderingContext* aRenderingContext,
                                         mozilla::WritingMode aWM,
                                         float aFontSizeInflation) const;

  nsresult ScrollSelectionIntoView() override;

private:
  //helper methods
  nsresult SetSelectionInternal(nsIDOMNode *aStartNode, int32_t aStartOffset,
                                nsIDOMNode *aEndNode, int32_t aEndOffset,
                                SelectionDirection aDirection = eNone);
  nsresult SelectAllOrCollapseToEndOfText(bool aSelect);
  nsresult SetSelectionEndPoints(int32_t aSelStart, int32_t aSelEnd,
                                 SelectionDirection aDirection = eNone);

  /**
   * Return the root DOM element, and implicitly initialize the editor if needed.
   */
  mozilla::dom::Element* GetRootNodeAndInitializeEditor();
  nsresult GetRootNodeAndInitializeEditor(nsIDOMElement **aRootElement);

  void FinishedInitializer() {
    Properties().Delete(TextControlInitializer());
  }

private:
  // these packed bools could instead use the high order bits on mState, saving 4 bytes 
  bool mEditorHasBeenInitialized;
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


