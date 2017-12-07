/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTextControlFrame_h___
#define nsTextControlFrame_h___

#include "mozilla/Attributes.h"
#include "mozilla/dom/Element.h"
#include "nsContainerFrame.h"
#include "nsIAnonymousContentCreator.h"
#include "nsIContent.h"
#include "nsITextControlFrame.h"
#include "nsITextControlElement.h"
#include "nsIStatefulFrame.h"

class nsISelectionController;
class EditorInitializerEntryTracker;
class nsTextEditorState;
namespace mozilla {
class TextEditor;
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
  NS_DECL_FRAMEARENA_HELPERS(nsTextControlFrame)

  NS_DECLARE_FRAME_PROPERTY_DELETABLE(ContentScrollPos, nsPoint)

  explicit nsTextControlFrame(nsStyleContext* aContext);
  virtual ~nsTextControlFrame();

  virtual void DestroyFrom(nsIFrame* aDestructRoot, PostDestroyData& aPostDestroyData) override;

  virtual nsIScrollableFrame* GetScrollTargetFrame() override {
    return do_QueryFrame(PrincipalChildList().FirstChild());
  }

  virtual nscoord GetMinISize(gfxContext* aRenderingContext) override;
  virtual nscoord GetPrefISize(gfxContext* aRenderingContext) override;

  virtual mozilla::LogicalSize
  ComputeAutoSize(gfxContext*                 aRenderingContext,
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

  bool GetVerticalAlignBaseline(mozilla::WritingMode aWM,
                                nscoord* aBaseline) const override
  {
    return GetNaturalBaselineBOffset(aWM, BaselineSharingGroup::eFirst, aBaseline);
  }

  bool GetNaturalBaselineBOffset(mozilla::WritingMode aWM,
                                 BaselineSharingGroup aBaselineGroup,
                                 nscoord* aBaseline) const override
  {
    if (!IsSingleLineTextControl()) {
      return false;
    }
    NS_ASSERTION(mFirstBaseline != NS_INTRINSIC_WIDTH_UNKNOWN,
                 "please call Reflow before asking for the baseline");
    if (aBaselineGroup == BaselineSharingGroup::eFirst) {
      *aBaseline = mFirstBaseline;
    } else {
      *aBaseline = BSize(aWM) - mFirstBaseline;
    }
    return true;
  }

  virtual nsSize GetXULMinSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual bool IsXULCollapsed() override;

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

#ifdef DEBUG
  void MarkIntrinsicISizesDirty() override
  {
    // Need another Reflow to have a correct baseline value again.
    mFirstBaseline = NS_INTRINSIC_WIDTH_UNKNOWN;
  }
#endif

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) override;
  virtual void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                        uint32_t aFilter) override;

  virtual void SetInitialChildList(ChildListID     aListID,
                                   nsFrameList&    aChildList) override;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsDisplayListSet& aLists) override;

  virtual mozilla::dom::Element*
  GetPseudoElement(mozilla::CSSPseudoElementType aType) override;

//==== BEGIN NSIFORMCONTROLFRAME
  virtual void SetFocus(bool aOn , bool aRepaint) override;
  virtual nsresult SetFormProperty(nsAtom* aName, const nsAString& aValue) override;

//==== END NSIFORMCONTROLFRAME

//==== NSITEXTCONTROLFRAME

  NS_IMETHOD_(already_AddRefed<mozilla::TextEditor>) GetTextEditor() override;
  NS_IMETHOD    SetSelectionRange(uint32_t aSelectionStart,
                                  uint32_t aSelectionEnd,
                                  SelectionDirection aDirection = eNone) override;
  NS_IMETHOD    GetOwnedSelectionController(nsISelectionController** aSelCon) override;
  virtual nsFrameSelection* GetOwnedFrameSelection() override;

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

  /** handler for attribute changes to mContent */
  virtual nsresult AttributeChanged(int32_t         aNameSpaceID,
                                    nsAtom*        aAttribute,
                                    int32_t         aModType) override;

  void GetText(nsString& aText);

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

  mozilla::dom::Element* GetRootNode() const {
    return mRootNode;
  }

  mozilla::dom::Element* GetPlaceholderNode() const {
    return mPlaceholderDiv;
  }

  mozilla::dom::Element* GetPreviewNode() const {
    return mPreviewDiv;
  }

  // called by the focus listener
  nsresult MaybeBeginSecureKeyboardInput();
  void MaybeEndSecureKeyboardInput();

#define DEFINE_TEXTCTRL_CONST_FORWARDER(type, name)                            \
  type name() const {                                                          \
    nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(GetContent()); \
    NS_ASSERTION(txtCtrl, "Content not a text control element");               \
    return txtCtrl->name();                                                    \
  }

  DEFINE_TEXTCTRL_CONST_FORWARDER(bool, IsSingleLineTextControl)
  DEFINE_TEXTCTRL_CONST_FORWARDER(bool, IsTextArea)
  DEFINE_TEXTCTRL_CONST_FORWARDER(bool, IsPasswordTextControl)
  DEFINE_TEXTCTRL_CONST_FORWARDER(int32_t, GetCols)
  DEFINE_TEXTCTRL_CONST_FORWARDER(int32_t, GetWrapCols)
  DEFINE_TEXTCTRL_CONST_FORWARDER(int32_t, GetRows)

#undef DEFINE_TEXTCTRL_CONST_FORWARDER

protected:
  class EditorInitializer;
  friend class EditorInitializer;
  friend class nsTextEditorState; // needs access to UpdateValueDisplay

  // Temp reference to scriptrunner
  NS_DECLARE_FRAME_PROPERTY_WITH_DTOR(TextControlInitializer,
                                      EditorInitializer,
                                      nsTextControlFrame::RevokeInitializer)

  static void
  RevokeInitializer(EditorInitializer* aInitializer) {
    aInitializer->Revoke();
  };

  class EditorInitializer : public mozilla::Runnable {
  public:
    explicit EditorInitializer(nsTextControlFrame* aFrame)
      : mozilla::Runnable("nsTextControlFrame::EditorInitializer")
      , mFrame(aFrame)
    {
    }

    NS_IMETHOD Run() override;

    // avoids use of AutoWeakFrame
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
    explicit ScrollOnFocusEvent(nsTextControlFrame* aFrame)
      : mozilla::Runnable("nsTextControlFrame::ScrollOnFocusEvent")
      , mFrame(aFrame)
    {
    }

    NS_DECL_NSIRUNNABLE

    void Revoke() {
      mFrame = nullptr;
    }

  private:
    nsTextControlFrame* mFrame;
  };

  nsresult OffsetToDOMPoint(uint32_t aOffset, nsIDOMNode** aResult, uint32_t* aPosition);

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
  bool AttributeExists(nsAtom *aAtt) const
  { return mContent && mContent->AsElement()->HasAttr(kNameSpaceID_None, aAtt); }

  /**
   * We call this when we are being destroyed or removed from the PFM.
   * @param aPresContext the current pres context
   */
  void PreDestroy();

  // Compute our intrinsic size.  This does not include any borders, paddings,
  // etc.  Just the size of our actual area for the text (and the scrollbars,
  // for <textarea>).
  mozilla::LogicalSize CalcIntrinsicSize(gfxContext* aRenderingContext,
                                         mozilla::WritingMode aWM,
                                         float aFontSizeInflation) const;

  nsresult ScrollSelectionIntoView() override;

private:
  //helper methods
  nsresult SetSelectionInternal(nsIDOMNode *aStartNode, uint32_t aStartOffset,
                                nsIDOMNode *aEndNode, uint32_t aEndOffset,
                                SelectionDirection aDirection = eNone);
  nsresult SelectAllOrCollapseToEndOfText(bool aSelect);
  nsresult SetSelectionEndPoints(uint32_t aSelStart, uint32_t aSelEnd,
                                 SelectionDirection aDirection = eNone);

  /**
   * Return the root DOM element, and implicitly initialize the editor if
   * needed.
   *
   * XXXbz This function is slow.  Very slow.  Consider using
   * EnsureEditorInitialized() if you need that, and
   * nsITextControlElement::GetRootEditorNode on our content if you need that.
   */
  nsresult GetRootNodeAndInitializeEditor(nsIDOMElement **aRootElement);

  void FinishedInitializer() {
    DeleteProperty(TextControlInitializer());
  }

  const nsAString& CachedValue() const
  {
    return mCachedValue;
  }

  void ClearCachedValue()
  {
    mCachedValue.SetIsVoid(true);
  }

  void CacheValue(const nsAString& aValue)
  {
    mCachedValue.Assign(aValue);
  }

  MOZ_MUST_USE bool
  CacheValue(const nsAString& aValue, const mozilla::fallible_t& aFallible)
  {
    if (!mCachedValue.Assign(aValue, aFallible)) {
      ClearCachedValue();
      return false;
    }
    return true;
  }

private:
  class nsAnonDivObserver;

  nsresult CreateRootNode();
  void CreatePlaceholderIfNeeded();
  void CreatePreviewIfNeeded();
  bool ShouldInitializeEagerly() const;
  void InitializeEagerlyIfNeeded();

  RefPtr<mozilla::dom::Element> mRootNode;
  RefPtr<mozilla::dom::Element> mPlaceholderDiv;
  RefPtr<mozilla::dom::Element> mPreviewDiv;
  RefPtr<nsAnonDivObserver> mMutationObserver;
  // Cache of the |.value| of <input> or <textarea> element without hard-wrap.
  // If its IsVoid() returns true, it doesn't cache |.value|.
  // Otherwise, it's cached when setting specific value or getting value from
  // TextEditor.  Additionally, when contents in the anonymous <div> element
  // is modified, this is cleared.
  //
  // FIXME(bug 1402545): Consider using an nsAutoString here.
  nsString mCachedValue;

  // Our first baseline, or NS_INTRINSIC_WIDTH_UNKNOWN if we have a pending
  // Reflow.
  nscoord mFirstBaseline;

  // these packed bools could instead use the high order bits on mState, saving 4 bytes
  bool mEditorHasBeenInitialized;
  bool mIsProcessing;

#ifdef DEBUG
  bool mInEditorInitialization;
  friend class EditorInitializerEntryTracker;
#endif

  nsRevocableEventPtr<ScrollOnFocusEvent> mScrollEvent;
};

#endif


