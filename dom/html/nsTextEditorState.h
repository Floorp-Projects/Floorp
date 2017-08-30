/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTextEditorState_h__
#define nsTextEditorState_h__

#include "nsString.h"
#include "nsITextControlElement.h"
#include "nsITextControlFrame.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/dom/Element.h"
#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/TextEditor.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/dom/HTMLInputElementBinding.h"
#include "mozilla/dom/Nullable.h"

class nsTextInputListener;
class nsTextControlFrame;
class nsTextInputSelectionImpl;
class nsAnonDivObserver;
class nsISelectionController;
class nsFrameSelection;
class nsITextControlElement;
class nsFrame;

namespace mozilla {

class ErrorResult;

namespace dom {
class HTMLInputElement;
} // namespace dom
} // namespace mozilla

/**
 * nsTextEditorState is a class which is responsible for managing the state of
 * plaintext controls.  This currently includes the following HTML elements:
 *   <input type=text>
 *   <input type=password>
 *   <textarea>
 * and also XUL controls such as <textbox> which use one of these elements behind
 * the scenes.
 *
 * This class is held as a member of HTMLInputElement and nsHTMLTextAreaElement.
 * The public functions in this class include the public APIs which content/ uses.
 * Layout code uses the nsITextControlElement interface to invoke functions on this
 * class.
 *
 * The design motivation behind this class is maintaining all of the things which
 * collectively are considered the "state" of the text control in a single location.
 * This state includes several things:
 *
 *  * The control's value.  This value is stored in the mValue member, and is only
 *    used when there is no frame for the control, or when the editor object has
 *    not been initialized yet.
 *
 *  * The control's associated frame.  This value is stored in the mBoundFrame member.
 *    A text control might never have an associated frame during its life cycle,
 *    or might have several different ones, but at any given moment in time there is
 *    a maximum of 1 bound frame to each text control.
 *
 *  * The control's associated editor.  This value is stored in the mEditor member.
 *    An editor is initilized for the control only when necessary (that is, when either
 *    the user is about to interact with the text control, or when some other code
 *    needs to access the editor object.  Without a frame bound to the control, an
 *    editor is never initialzied.  Once initialized, the editor might outlive the frame,
 *    in which case the same editor will be used if a new frame gets bound to the
 *    text control.
 *
 *  * The anonymous content associated with the text control's frame, including the
 *    value div (the DIV element responsible for holding the value of the text control)
 *    and the placeholder div (the DIV element responsible for holding the placeholder
 *    value of the text control.)  These values are stored in the mRootNode and
 *    mPlaceholderDiv members, respectively.  They will be created when a
 *    frame is bound to the text control.  They will be destroyed when the frame is
 *    unbound from the object.  We could try and hold on to the anonymous content
 *    between different frames, but unfortunately that is not currently possible
 *    because they are not unbound from the document in time.
 *
 *  * The frame selection controller.  This value is stored in the mSelCon member.
 *    The frame selection controller is responsible for maintaining the selection state
 *    on a frame.  It is created when a frame is bound to the text control element,
 *    and will be destroy when the frame is being unbound from the text control element.
 *    It is created alongside with the frame selection object which is stored in the
 *    mFrameSel member.
 *
 *  * The editor text listener.  This value is stored in the mTextListener member.
 *    Its job is to listen to selection and keyboard events, and act accordingly.
 *    It is created when an a frame is first bound to the control, and will be destroyed
 *    when the frame is unbound from the text control element.
 *
 *  * The editor's cached value.  This value is stored in the mCachedValue member.
 *    It is used to improve the performance of append operations to the text
 *    control.  A mutation observer stored in the mMutationObserver has the job of
 *    invalidating this cache when the anonymous contect containing the value is
 *    changed.
 *
 *  * The editor's cached selection properties.  These vales are stored in the
 *    mSelectionProperties member, and include the selection's start, end and
 *    direction. They are only used when there is no frame available for the
 *    text field.
 *
 *
 * As a general rule, nsTextEditorState objects own the value of the text control, and any
 * attempt to retrieve or set the value must be made through those objects.  Internally,
 * the value can be represented in several different ways, based on the state the control is
 * in.
 *
 *   * When the control is first initialized, its value is equal to the default value of
 *     the DOM node.  For <input> text controls, this default value is the value of the
 *     value attribute.  For <textarea> elements, this default value is the value of the
 *     text node children of the element.
 *
 *   * If the value has been changed through the DOM node (before the editor for the object
 *     is initialized), the value is stored as a simple string inside the mValue member of
 *     the nsTextEditorState object.
 *
 *   * If an editor has been initialized for the control, the value is set and retrievd via
 *     the nsIPlaintextEditor interface, and is internally managed by the editor as the
 *     native anonymous content tree attached to the control's frame.
 *
 *   * If the text editor state object is unbound from the control's frame, the value is
 *     transferred to the mValue member variable, and will be managed there until a new
 *     frame is bound to the text editor state object.
 */

class RestoreSelectionState;

class nsTextEditorState : public mozilla::SupportsWeakPtr<nsTextEditorState> {
public:
  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(nsTextEditorState)
  explicit nsTextEditorState(nsITextControlElement* aOwningElement);
  static nsTextEditorState*
  Construct(nsITextControlElement* aOwningElement,
            nsTextEditorState** aReusedState);
  ~nsTextEditorState();

  void Traverse(nsCycleCollectionTraversalCallback& cb);
  void Unlink();

  void PrepareForReuse()
  {
    Unlink();
    mValue.reset();
    mCachedValue.Truncate();
    mValueBeingSet.Truncate();
    mTextCtrlElement = nullptr;
    MOZ_ASSERT(!mMutationObserver);
  }

  mozilla::TextEditor* GetTextEditor();
  nsISelectionController* GetSelectionController() const;
  nsFrameSelection* GetConstFrameSelection();
  nsresult BindToFrame(nsTextControlFrame* aFrame);
  void UnbindFromFrame(nsTextControlFrame* aFrame);
  nsresult PrepareEditor(const nsAString *aValue = nullptr);
  void InitializeKeyboardEventListeners();

  enum SetValueFlags
  {
    // The call is for internal processing.
    eSetValue_Internal              = 0,
    // The value is changed by a call of setUserInput() from chrome.
    eSetValue_BySetUserInput        = 1 << 0,
    // The value is changed by changing value attribute of the element or
    // something like setRangeText().
    eSetValue_ByContent             = 1 << 1,
    // Whether the value change should be notified to the frame/contet nor not.
    eSetValue_Notify                = 1 << 2,
    // Whether to move the cursor to end of the value (in the case when we have
    // cached selection offsets), in the case when the value has changed.  If
    // this is not set, the cached selection offsets will simply be clamped to
    // be within the length of the new value.  In either case, if the value has
    // not changed the cursor won't move.
    eSetValue_MoveCursorToEndIfValueChanged = 1 << 3,
    // The value is changed for a XUL text control as opposed to for an HTML
    // text control.  Such value changes are different in that they preserve the
    // undo history.
    eSetValue_ForXUL                = 1 << 4,
  };
  MOZ_MUST_USE bool SetValue(const nsAString& aValue,
                             const nsAString* aOldValue,
                             uint32_t aFlags);
  MOZ_MUST_USE bool SetValue(const nsAString& aValue,
                             uint32_t aFlags)
  {
    return SetValue(aValue, nullptr, aFlags);
  }
  void GetValue(nsAString& aValue, bool aIgnoreWrap) const;
  bool HasNonEmptyValue();
  // The following methods are for textarea element to use whether default
  // value or not.
  // XXX We might have to add assertion when it is into editable,
  // or reconsider fixing bug 597525 to remove these.
  void EmptyValue() { if (mValue) mValue->Truncate(); }
  bool IsEmpty() const { return mValue ? mValue->IsEmpty() : true; }

  nsresult CreatePlaceholderNode();
  nsresult CreatePreviewNode();
  mozilla::dom::Element* CreateEmptyDivNode();

  mozilla::dom::Element* GetRootNode() {
    return mRootNode;
  }
  mozilla::dom::Element* GetPlaceholderNode() {
    return mPlaceholderDiv;
  }
  mozilla::dom::Element* GetPreviewNode() {
    return mPreviewDiv;
  }

  bool IsSingleLineTextControl() const {
    return mTextCtrlElement->IsSingleLineTextControl();
  }
  bool IsTextArea() const {
    return mTextCtrlElement->IsTextArea();
  }
  bool IsPasswordTextControl() const {
    return mTextCtrlElement->IsPasswordTextControl();
  }
  int32_t GetCols() {
    return mTextCtrlElement->GetCols();
  }
  int32_t GetWrapCols() {
    return mTextCtrlElement->GetWrapCols();
  }
  int32_t GetRows() {
    return mTextCtrlElement->GetRows();
  }

  void UpdateOverlayTextVisibility(bool aNotify);

  // placeholder methods
  bool GetPlaceholderVisibility() {
    return mPlaceholderVisibility;
  }
  void UpdatePlaceholderText(bool aNotify);

  // preview methods
  void SetPreviewText(const nsAString& aValue, bool aNotify);
  void GetPreviewText(nsAString& aValue);
  bool GetPreviewVisibility() {
    return mPreviewVisibility;
  }

  /**
   * Get the maxlength attribute
   * @param aMaxLength the value of the max length attr
   * @returns false if attr not defined
   */
  int32_t GetMaxLength();

  void ClearValueCache() { mCachedValue.Truncate(); }

  void HideSelectionIfBlurred();

  struct SelectionProperties {
    public:
      SelectionProperties() : mStart(0), mEnd(0),
        mDirection(nsITextControlFrame::eForward) {}
      bool IsDefault() const
      {
        return mStart == 0 && mEnd == 0 &&
               mDirection == nsITextControlFrame::eForward;
      }
      uint32_t GetStart() const
      {
        return mStart;
      }
      void SetStart(uint32_t value)
      {
        mIsDirty = true;
        mStart = value;
      }
      uint32_t GetEnd() const
      {
        return mEnd;
      }
      void SetEnd(uint32_t value)
      {
        mIsDirty = true;
        mEnd = value;
      }
      nsITextControlFrame::SelectionDirection GetDirection() const
      {
        return mDirection;
      }
      void SetDirection(nsITextControlFrame::SelectionDirection value)
      {
        mIsDirty = true;
        mDirection = value;
      }
      // return true only if mStart, mEnd, or mDirection have been modified,
      // or if SetIsDirty() was explicitly called.
      bool IsDirty() const
      {
        return mIsDirty;
      }
      void SetIsDirty()
      {
        mIsDirty = true;
      }
    private:
      uint32_t mStart, mEnd;
      bool mIsDirty = false;
      nsITextControlFrame::SelectionDirection mDirection;
  };

  bool IsSelectionCached() const;
  SelectionProperties& GetSelectionProperties();
  void SetSelectionProperties(SelectionProperties& aProps);
  void WillInitEagerly() { mSelectionRestoreEagerInit = true; }
  bool HasNeverInitializedBefore() const { return !mEverInited; }
  // Sync up our selection properties with our editor prior to being destroyed.
  // This will invoke UnbindFromFrame() to ensure that we grab whatever
  // selection state may be at the moment.
  void SyncUpSelectionPropertiesBeforeDestruction();

  // Get the selection range start and end points in our text.
  void GetSelectionRange(uint32_t* aSelectionStart, uint32_t* aSelectionEnd,
                         mozilla::ErrorResult& aRv);

  // Get the selection direction
  nsITextControlFrame::SelectionDirection
    GetSelectionDirection(mozilla::ErrorResult& aRv);

  // Set the selection range (start, end, direction).  aEnd is allowed to be
  // smaller than aStart; in that case aStart will be reset to the same value as
  // aEnd.  This basically implements
  // https://html.spec.whatwg.org/multipage/forms.html#set-the-selection-range
  // but with the start/end already coerced to zero if null (and without the
  // special infinity value), and the direction already converted to a
  // SelectionDirection.
  //
  // If we have a frame, this method will scroll the selection into view.
  //
  // XXXbz This should really take uint32_t, but none of our guts (either the
  // frame or our cached selection state) work with uint32_t at the moment...
  void SetSelectionRange(uint32_t aStart, uint32_t aEnd,
                         nsITextControlFrame::SelectionDirection aDirection,
                         mozilla::ErrorResult& aRv);

  // Set the selection range, but with an optional string for the direction.
  // This will convert aDirection to an nsITextControlFrame::SelectionDirection
  // and then call our other SetSelectionRange overload.
  void SetSelectionRange(uint32_t aSelectionStart,
                         uint32_t aSelectionEnd,
                         const mozilla::dom::Optional<nsAString>& aDirection,
                         mozilla::ErrorResult& aRv);

  // Set the selection start.  This basically implements the
  // https://html.spec.whatwg.org/multipage/forms.html#dom-textarea/input-selectionstart
  // setter.
  void SetSelectionStart(const mozilla::dom::Nullable<uint32_t>& aStart,
                         mozilla::ErrorResult& aRv);

  // Set the selection end.  This basically implements the
  // https://html.spec.whatwg.org/multipage/forms.html#dom-textarea/input-selectionend
  // setter.
  void SetSelectionEnd(const mozilla::dom::Nullable<uint32_t>& aEnd,
                       mozilla::ErrorResult& aRv);

  // Get the selection direction as a string.  This implements the
  // https://html.spec.whatwg.org/multipage/forms.html#dom-textarea/input-selectiondirection
  // getter.
  void GetSelectionDirectionString(nsAString& aDirection,
                                   mozilla::ErrorResult& aRv);

  // Set the selection direction.  This basically implements the
  // https://html.spec.whatwg.org/multipage/forms.html#dom-textarea/input-selectiondirection
  // setter.
  void SetSelectionDirection(const nsAString& aDirection,
                             mozilla::ErrorResult& aRv);

  // Set the range text.  This basically implements
  // https://html.spec.whatwg.org/multipage/forms.html#dom-textarea/input-setrangetext
  void SetRangeText(const nsAString& aReplacement, mozilla::ErrorResult& aRv);
  // The last two arguments are -1 if we don't know our selection range;
  // otherwise they're the start and end of our selection range.
  void SetRangeText(const nsAString& aReplacement, uint32_t aStart,
                    uint32_t aEnd, mozilla::dom::SelectionMode aSelectMode,
                    mozilla::ErrorResult& aRv,
                    const mozilla::Maybe<uint32_t>& aSelectionStart =
                      mozilla::Nothing(),
                    const mozilla::Maybe<uint32_t>& aSelectionEnd =
                      mozilla::Nothing());

  void UpdateEditableState(bool aNotify) {
    if (mRootNode) {
      mRootNode->UpdateEditableState(aNotify);
    }
  }

private:
  friend class RestoreSelectionState;

  // not copy constructible
  nsTextEditorState(const nsTextEditorState&);
  // not assignable
  void operator= (const nsTextEditorState&);

  nsresult CreateRootNode();

  void ValueWasChanged(bool aNotify);

  void DestroyEditor();
  void Clear();

  nsresult InitializeRootNode();

  void FinishedRestoringSelection();

  mozilla::dom::HTMLInputElement* GetParentNumberControl(nsFrame* aFrame) const;

  bool EditorHasComposition();

  class InitializationGuard {
  public:
    explicit InitializationGuard(nsTextEditorState& aState) :
      mState(aState),
      mGuardSet(false)
    {
      if (!mState.mInitializing) {
        mGuardSet = true;
        mState.mInitializing = true;
      }
    }
    ~InitializationGuard() {
      if (mGuardSet) {
        mState.mInitializing = false;
      }
    }
    bool IsInitializingRecursively() const {
      return !mGuardSet;
    }
  private:
    nsTextEditorState& mState;
    bool mGuardSet;
  };
  friend class InitializationGuard;
  friend class PrepareEditorEvent;

  // The text control element owns this object, and ensures that this object
  // has a smaller lifetime.
  nsITextControlElement* MOZ_NON_OWNING_REF mTextCtrlElement;
  // mSelCon is non-null while we have an mBoundFrame.
  RefPtr<nsTextInputSelectionImpl> mSelCon;
  RefPtr<RestoreSelectionState> mRestoringSelection;
  RefPtr<mozilla::TextEditor> mTextEditor;
  nsCOMPtr<mozilla::dom::Element> mRootNode;
  nsCOMPtr<mozilla::dom::Element> mPlaceholderDiv;
  nsCOMPtr<mozilla::dom::Element> mPreviewDiv;
  nsTextControlFrame* mBoundFrame;
  RefPtr<nsTextInputListener> mTextListener;
  mozilla::Maybe<nsString> mValue;
  RefPtr<nsAnonDivObserver> mMutationObserver;
  mutable nsString mCachedValue; // Caches non-hard-wrapped value on a multiline control.
  // mValueBeingSet is available only while SetValue() is requesting to commit
  // composition.  I.e., this is valid only while mIsCommittingComposition is
  // true.  While active composition is being committed, GetValue() needs
  // the latest value which is set by SetValue().  So, this is cache for that.
  nsString mValueBeingSet;
  SelectionProperties mSelectionProperties;
  bool mEverInited; // Have we ever been initialized?
  bool mEditorInitialized;
  bool mInitializing; // Whether we're in the process of initialization
  bool mValueTransferInProgress; // Whether a value is being transferred to the frame
  bool mSelectionCached; // Whether mSelectionProperties is valid
  mutable bool mSelectionRestoreEagerInit; // Whether we're eager initing because of selection restore
  bool mPlaceholderVisibility;
  bool mPreviewVisibility;
  bool mIsCommittingComposition;
};

inline void
ImplCycleCollectionUnlink(nsTextEditorState& aField)
{
  aField.Unlink();
}

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            nsTextEditorState& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  aField.Traverse(aCallback);
}

#endif
