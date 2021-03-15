/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TextControlState_h
#define mozilla_TextControlState_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/EnumSet.h"
#include "mozilla/Maybe.h"
#include "mozilla/TextControlElement.h"
#include "mozilla/TextEditor.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/dom/HTMLInputElementBinding.h"
#include "mozilla/dom/Nullable.h"
#include "nsCycleCollectionParticipant.h"
#include "nsITextControlFrame.h"

class nsTextControlFrame;
class nsISelectionController;
class nsFrameSelection;
class nsFrame;

namespace mozilla {

class AutoTextControlHandlingState;
class ErrorResult;
class TextInputListener;
class TextInputSelectionController;

namespace dom {
class Element;
class HTMLInputElement;
}  // namespace dom

/**
 * TextControlState is a class which is responsible for managing the state of
 * plaintext controls.  This currently includes the following HTML elements:
 *   <input type=text>
 *   <input type=search>
 *   <input type=url>
 *   <input type=telephone>
 *   <input type=email>
 *   <input type=password>
 *   <textarea>
 *
 * This class is held as a member of HTMLInputElement and HTMLTextAreaElement.
 * The public functions in this class include the public APIs which dom/
 * uses. Layout code uses the TextControlElement interface to invoke
 * functions on this class.
 *
 * The design motivation behind this class is maintaining all of the things
 * which collectively are considered the "state" of the text control in a single
 * location. This state includes several things:
 *
 *  * The control's value.  This value is stored in the mValue member, and is
 * only used when there is no frame for the control, or when the editor object
 * has not been initialized yet.
 *
 *  * The control's associated frame.  This value is stored in the mBoundFrame
 * member. A text control might never have an associated frame during its life
 * cycle, or might have several different ones, but at any given moment in time
 * there is a maximum of 1 bound frame to each text control.
 *
 *  * The control's associated editor.  This value is stored in the mTextEditor
 * member. An editor is initialized for the control only when necessary (that
 * is, when either the user is about to interact with the text control, or when
 * some other code needs to access the editor object.  Without a frame bound to
 * the control, an editor is never initialized.  Once initialized, the editor
 * might outlive the frame, in which case the same editor will be used if a new
 * frame gets bound to the text control.
 *
 *  * The anonymous content associated with the text control's frame, including
 * the value div (the DIV element responsible for holding the value of the text
 * control) and the placeholder div (the DIV element responsible for holding the
 * placeholder value of the text control.)  These values are stored in the
 * mRootNode and mPlaceholderDiv members, respectively.  They will be created
 * when a frame is bound to the text control.  They will be destroyed when the
 * frame is unbound from the object.  We could try and hold on to the anonymous
 * content between different frames, but unfortunately that is not currently
 * possible because they are not unbound from the document in time.
 *
 *  * The frame selection controller.  This value is stored in the mSelCon
 * member. The frame selection controller is responsible for maintaining the
 * selection state on a frame.  It is created when a frame is bound to the text
 * control element, and will be destroy when the frame is being unbound from the
 * text control element. It is created alongside with the frame selection object
 * which is stored in the mFrameSel member.
 *
 *  * The editor text listener.  This value is stored in the mTextListener
 * member. Its job is to listen to selection and keyboard events, and act
 * accordingly. It is created when an a frame is first bound to the control, and
 * will be destroyed when the frame is unbound from the text control element.
 *
 *  * The editor's cached value.  This value is stored in the mCachedValue
 * member. It is used to improve the performance of append operations to the
 * text control.  A mutation observer stored in the mMutationObserver has the
 * job of invalidating this cache when the anonymous contect containing the
 * value is changed.
 *
 *  * The editor's cached selection properties.  These vales are stored in the
 *    mSelectionProperties member, and include the selection's start, end and
 *    direction. They are only used when there is no frame available for the
 *    text field.
 *
 *
 * As a general rule, TextControlState objects own the value of the text
 * control, and any attempt to retrieve or set the value must be made through
 * those objects.  Internally, the value can be represented in several different
 * ways, based on the state the control is in.
 *
 *   * When the control is first initialized, its value is equal to the default
 * value of the DOM node.  For <input> text controls, this default value is the
 * value of the value attribute.  For <textarea> elements, this default value is
 * the value of the text node children of the element.
 *
 *   * If the value has been changed through the DOM node (before the editor for
 * the object is initialized), the value is stored as a simple string inside the
 * mValue member of the TextControlState object.
 *
 *   * If an editor has been initialized for the control, the value is set and
 * retrievd via the nsIEditor interface, and is internally managed by the
 * editor as the native anonymous content tree attached to the control's frame.
 *
 *   * If the text control state object is unbound from the control's frame, the
 * value is transferred to the mValue member variable, and will be managed there
 * until a new frame is bound to the text editor state object.
 */

class RestoreSelectionState;

class TextControlState final : public SupportsWeakPtr {
 public:
  typedef dom::Element Element;
  typedef dom::HTMLInputElement HTMLInputElement;

  static TextControlState* Construct(TextControlElement* aOwningElement);

  // Note that this does not run script actually because of `sHasShutDown`
  // is set to true before calling `DeleteOrCacheForReuse()`.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY static void Shutdown();

  /**
   * Destroy() deletes the instance immediately or later.
   */
  MOZ_CAN_RUN_SCRIPT void Destroy();

  TextControlState() = delete;
  explicit TextControlState(const TextControlState&) = delete;
  TextControlState(TextControlState&&) = delete;

  void operator=(const TextControlState&) = delete;
  void operator=(TextControlState&&) = delete;

  void Traverse(nsCycleCollectionTraversalCallback& cb);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void Unlink();

  bool IsBusy() const { return !!mHandlingState || mValueTransferInProgress; }

  MOZ_CAN_RUN_SCRIPT TextEditor* GetTextEditor();
  TextEditor* GetTextEditorWithoutCreation();
  nsISelectionController* GetSelectionController() const;
  nsFrameSelection* GetConstFrameSelection();
  nsresult BindToFrame(nsTextControlFrame* aFrame);
  MOZ_CAN_RUN_SCRIPT void UnbindFromFrame(nsTextControlFrame* aFrame);
  MOZ_CAN_RUN_SCRIPT nsresult PrepareEditor(const nsAString* aValue = nullptr);
  void InitializeKeyboardEventListeners();

  /**
   * OnEditActionHandled() is called when mTextEditor handles something
   * and immediately before dispatching "input" event.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult OnEditActionHandled();

  enum class ValueSetterOption {
    // The call is for setting value to initial one, computed one, etc.
    ByInternalAPI,
    // The value is changed by a call of setUserInput() API from chrome.
    BySetUserInputAPI,
    // The value is changed by changing value attribute of the element or
    // something like setRangeText().
    ByContentAPI,
    // Whether SetValueChanged should be called as a result of this value
    // change.
    SetValueChanged,
    // Whether to move the cursor to end of the value (in the case when we have
    // cached selection offsets), in the case when the value has changed.  If
    // this is not set and MoveCursorToBeginSetSelectionDirectionForward
    // is not set, the cached selection offsets will simply be clamped to
    // be within the length of the new value. In either case, if the value has
    // not changed the cursor won't move.
    // TODO(mbrodesser): update comment and enumerator identifier to reflect
    // that also the direction is set to forward.
    MoveCursorToEndIfValueChanged,

    // The value change should preserve undo history.
    PreserveUndoHistory,

    // Whether it should be tried to move the cursor to the beginning of the
    // text control and set the selection direction to "forward".
    // TODO(mbrodesser): As soon as "none" is supported
    // (https://bugzilla.mozilla.org/show_bug.cgi?id=1541454), it should be set
    // to "none" and only fall back to "forward" if the platform doesn't support
    // it.
    MoveCursorToBeginSetSelectionDirectionForward,
  };
  using ValueSetterOptions = EnumSet<ValueSetterOption, uint32_t>;

  /**
   * SetValue() sets the value to aValue with replacing \r\n and \r with \n.
   *
   * @param aValue      The new value.  Can contain \r.
   * @param aOldValue   Optional.  If you have already know current value,
   *                    set this to it.  However, this must not contain \r
   *                    for the performance.
   * @param aOptions    See ValueSetterOption.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT bool SetValue(
      const nsAString& aValue, const nsAString* aOldValue,
      const ValueSetterOptions& aOptions);
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT bool SetValue(
      const nsAString& aValue, const ValueSetterOptions& aOptions) {
    return SetValue(aValue, nullptr, aOptions);
  }

  /**
   * GetValue() returns current value either with or without TextEditor.
   * The result never includes \r.
   */
  void GetValue(nsAString& aValue, bool aIgnoreWrap) const;
  /**
   * ValueEquals() is designed for internal use so that aValue shouldn't
   * include \r character.  It should be handled before calling this with
   * nsContentUtils::PlatformToDOMLineBreaks().
   */
  bool ValueEquals(const nsAString& aValue) const;
  bool HasNonEmptyValue();
  // The following methods are for textarea element to use whether default
  // value or not.
  // XXX We might have to add assertion when it is into editable,
  // or reconsider fixing bug 597525 to remove these.
  void EmptyValue() {
    if (mValue) {
      mValue->Truncate();
    }
  }
  bool IsEmpty() const { return mValue ? mValue->IsEmpty() : true; }

  Element* GetRootNode();
  Element* GetPreviewNode();

  bool IsSingleLineTextControl() const {
    return mTextCtrlElement->IsSingleLineTextControl();
  }
  bool IsTextArea() const { return mTextCtrlElement->IsTextArea(); }
  bool IsPasswordTextControl() const {
    return mTextCtrlElement->IsPasswordTextControl();
  }
  int32_t GetCols() { return mTextCtrlElement->GetCols(); }
  int32_t GetWrapCols() {
    int32_t wrapCols = mTextCtrlElement->GetWrapCols();
    MOZ_ASSERT(wrapCols >= 0);
    return wrapCols;
  }
  int32_t GetRows() { return mTextCtrlElement->GetRows(); }

  // preview methods
  void SetPreviewText(const nsAString& aValue, bool aNotify);
  void GetPreviewText(nsAString& aValue);

  struct SelectionProperties {
   public:
    bool IsDefault() const {
      return mStart == 0 && mEnd == 0 &&
             mDirection == nsITextControlFrame::eForward;
    }
    uint32_t GetStart() const { return mStart; }
    bool SetStart(uint32_t value) {
      uint32_t newValue = std::min(value, *mMaxLength);
      bool changed = mStart != newValue;
      mStart = newValue;
      mIsDirty |= changed;
      return changed;
    }
    uint32_t GetEnd() const { return mEnd; }
    bool SetEnd(uint32_t value) {
      uint32_t newValue = std::min(value, *mMaxLength);
      bool changed = mEnd != newValue;
      mEnd = newValue;
      mIsDirty |= changed;
      return changed;
    }
    nsITextControlFrame::SelectionDirection GetDirection() const {
      return mDirection;
    }
    bool SetDirection(nsITextControlFrame::SelectionDirection value) {
      bool changed = mDirection != value;
      mDirection = value;
      mIsDirty |= changed;
      return changed;
    }
    void SetMaxLength(uint32_t aMax) {
      mMaxLength = Some(aMax);
      // recompute against the new max length
      SetStart(GetStart());
      SetEnd(GetEnd());
    }
    bool HasMaxLength() { return mMaxLength.isSome(); }

    // return true only if mStart, mEnd, or mDirection have been modified,
    // or if SetIsDirty() was explicitly called.
    bool IsDirty() const { return mIsDirty; }
    void SetIsDirty() { mIsDirty = true; }

   private:
    uint32_t mStart = 0;
    uint32_t mEnd = 0;
    Maybe<uint32_t> mMaxLength;
    bool mIsDirty = false;
    nsITextControlFrame::SelectionDirection mDirection =
        nsITextControlFrame::eForward;
  };

  bool IsSelectionCached() const { return mSelectionCached; }
  SelectionProperties& GetSelectionProperties() { return mSelectionProperties; }
  MOZ_CAN_RUN_SCRIPT void SetSelectionProperties(SelectionProperties& aProps);
  bool HasNeverInitializedBefore() const { return !mEverInited; }
  // Sync up our selection properties with our editor prior to being destroyed.
  // This will invoke UnbindFromFrame() to ensure that we grab whatever
  // selection state may be at the moment.
  MOZ_CAN_RUN_SCRIPT void SyncUpSelectionPropertiesBeforeDestruction();

  // Get the selection range start and end points in our text.
  void GetSelectionRange(uint32_t* aSelectionStart, uint32_t* aSelectionEnd,
                         ErrorResult& aRv);

  // Get the selection direction
  nsITextControlFrame::SelectionDirection GetSelectionDirection(
      ErrorResult& aRv);

  enum class ScrollAfterSelection { No, Yes };

  // Set the selection range (start, end, direction).  aEnd is allowed to be
  // smaller than aStart; in that case aStart will be reset to the same value as
  // aEnd.  This basically implements
  // https://html.spec.whatwg.org/multipage/forms.html#set-the-selection-range
  // but with the start/end already coerced to zero if null (and without the
  // special infinity value), and the direction already converted to a
  // SelectionDirection.
  //
  // If we have a frame, this method will scroll the selection into view.
  MOZ_CAN_RUN_SCRIPT void SetSelectionRange(
      uint32_t aStart, uint32_t aEnd,
      nsITextControlFrame::SelectionDirection aDirection, ErrorResult& aRv,
      ScrollAfterSelection aScroll = ScrollAfterSelection::Yes);

  // Set the selection range, but with an optional string for the direction.
  // This will convert aDirection to an nsITextControlFrame::SelectionDirection
  // and then call our other SetSelectionRange overload.
  MOZ_CAN_RUN_SCRIPT void SetSelectionRange(
      uint32_t aSelectionStart, uint32_t aSelectionEnd,
      const dom::Optional<nsAString>& aDirection, ErrorResult& aRv,
      ScrollAfterSelection aScroll = ScrollAfterSelection::Yes);

  // Set the selection start.  This basically implements the
  // https://html.spec.whatwg.org/multipage/forms.html#dom-textarea/input-selectionstart
  // setter.
  MOZ_CAN_RUN_SCRIPT void SetSelectionStart(
      const dom::Nullable<uint32_t>& aStart, ErrorResult& aRv);

  // Set the selection end.  This basically implements the
  // https://html.spec.whatwg.org/multipage/forms.html#dom-textarea/input-selectionend
  // setter.
  MOZ_CAN_RUN_SCRIPT void SetSelectionEnd(const dom::Nullable<uint32_t>& aEnd,
                                          ErrorResult& aRv);

  // Get the selection direction as a string.  This implements the
  // https://html.spec.whatwg.org/multipage/forms.html#dom-textarea/input-selectiondirection
  // getter.
  void GetSelectionDirectionString(nsAString& aDirection, ErrorResult& aRv);

  // Set the selection direction.  This basically implements the
  // https://html.spec.whatwg.org/multipage/forms.html#dom-textarea/input-selectiondirection
  // setter.
  MOZ_CAN_RUN_SCRIPT void SetSelectionDirection(const nsAString& aDirection,
                                                ErrorResult& aRv);

  // Set the range text.  This basically implements
  // https://html.spec.whatwg.org/multipage/forms.html#dom-textarea/input-setrangetext
  MOZ_CAN_RUN_SCRIPT void SetRangeText(const nsAString& aReplacement,
                                       ErrorResult& aRv);
  // The last two arguments are -1 if we don't know our selection range;
  // otherwise they're the start and end of our selection range.
  MOZ_CAN_RUN_SCRIPT void SetRangeText(
      const nsAString& aReplacement, uint32_t aStart, uint32_t aEnd,
      dom::SelectionMode aSelectMode, ErrorResult& aRv,
      const Maybe<uint32_t>& aSelectionStart = Nothing(),
      const Maybe<uint32_t>& aSelectionEnd = Nothing());

 private:
  explicit TextControlState(TextControlElement* aOwningElement);
  MOZ_CAN_RUN_SCRIPT ~TextControlState();

  /**
   * Delete the instance or cache to reuse it if possible.
   */
  MOZ_CAN_RUN_SCRIPT void DeleteOrCacheForReuse();

  MOZ_CAN_RUN_SCRIPT void UnlinkInternal();

  MOZ_CAN_RUN_SCRIPT void DestroyEditor();
  MOZ_CAN_RUN_SCRIPT void Clear();

  nsresult InitializeRootNode();

  void FinishedRestoringSelection();

  bool EditorHasComposition();

  /**
   * SetValueWithTextEditor() modifies the editor value with mTextEditor.
   * This may cause destroying mTextEditor, mBoundFrame, the TextControlState
   * itself.  Must be called when both mTextEditor and mBoundFrame are not
   * nullptr.
   *
   * @param aHandlingState      Must be inner-most handling state for SetValue.
   * @return                    false if fallible allocation failed.  Otherwise,
   *                            true.
   */
  MOZ_CAN_RUN_SCRIPT bool SetValueWithTextEditor(
      AutoTextControlHandlingState& aHandlingState);

  /**
   * SetValueWithoutTextEditor() modifies the value without editor.  I.e.,
   * modifying the value in this instance and mBoundFrame.  Must be called
   * when at least mTextEditor or mBoundFrame is nullptr.
   *
   * @param aHandlingState      Must be inner-most handling state for SetValue.
   * @return                    false if fallible allocation failed.  Otherwise,
   *                            true.
   */
  MOZ_CAN_RUN_SCRIPT bool SetValueWithoutTextEditor(
      AutoTextControlHandlingState& aHandlingState);

  // When this class handles something which may run script, this should be
  // set to non-nullptr.  If so, this class claims that it's busy and that
  // prevents destroying TextControlState instance.
  AutoTextControlHandlingState* mHandlingState = nullptr;

  // The text control element owns this object, and ensures that this object
  // has a smaller lifetime except the owner releases the instance while it
  // does something with this.
  TextControlElement* MOZ_NON_OWNING_REF mTextCtrlElement;
  RefPtr<TextInputSelectionController> mSelCon;
  RefPtr<RestoreSelectionState> mRestoringSelection;
  RefPtr<TextEditor> mTextEditor;
  nsTextControlFrame* mBoundFrame;
  RefPtr<TextInputListener> mTextListener;
  Maybe<nsString> mValue;
  SelectionProperties mSelectionProperties;
  bool mEverInited;  // Have we ever been initialized?
  bool mEditorInitialized;
  bool mValueTransferInProgress;  // Whether a value is being transferred to the
                                  // frame
  bool mSelectionCached;          // Whether mSelectionProperties is valid

  /**
   * For avoiding allocation cost of the instance, we should reuse instances
   * as far as possible.
   *
   * FYI: `25` is just a magic number considered without enough investigation,
   *      but at least, this value must not make damage for footprint.
   *      Feel free to change it if you find better number.
   */
  static const size_t kMaxCountOfCacheToReuse = 25;
  static AutoTArray<TextControlState*, kMaxCountOfCacheToReuse>*
      sReleasedInstances;
  static bool sHasShutDown;

  friend class AutoTextControlHandlingState;
  friend class PrepareEditorEvent;
  friend class RestoreSelectionState;
};

}  // namespace mozilla

#endif  // #ifndef mozilla_TextControlState_h
