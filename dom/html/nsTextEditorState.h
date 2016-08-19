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
#include "mozilla/WeakPtr.h"

class nsTextInputListener;
class nsTextControlFrame;
class nsTextInputSelectionImpl;
class nsAnonDivObserver;
class nsISelectionController;
class nsFrameSelection;
class nsIEditor;
class nsITextControlElement;
class nsFrame;

namespace mozilla {
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
  ~nsTextEditorState();

  void Traverse(nsCycleCollectionTraversalCallback& cb);
  void Unlink();

  nsIEditor* GetEditor();
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
    eSetValue_Notify                = 1 << 2
  };
  MOZ_MUST_USE bool SetValue(const nsAString& aValue, uint32_t aFlags);
  void GetValue(nsAString& aValue, bool aIgnoreWrap) const;
  void EmptyValue() { if (mValue) mValue->Truncate(); }
  bool IsEmpty() const { return mValue ? mValue->IsEmpty() : true; }

  nsresult CreatePlaceholderNode();

  mozilla::dom::Element* GetRootNode() {
    return mRootNode;
  }
  mozilla::dom::Element* GetPlaceholderNode() {
    return mPlaceholderDiv;
  }

  bool IsSingleLineTextControl() const {
    return mTextCtrlElement->IsSingleLineTextControl();
  }
  bool IsTextArea() const {
    return mTextCtrlElement->IsTextArea();
  }
  bool IsPlainTextControl() const {
    return mTextCtrlElement->IsPlainTextControl();
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

  // placeholder methods
  void UpdatePlaceholderVisibility(bool aNotify);
  bool GetPlaceholderVisibility() {
    return mPlaceholderVisibility;
  }
  void UpdatePlaceholderText(bool aNotify);

  /**
   * Get the maxlength attribute
   * @param aMaxLength the value of the max length attr
   * @returns false if attr not defined
   */
  bool GetMaxLength(int32_t* aMaxLength);

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
      int32_t GetStart() const
      {
        return mStart;
      }
      void SetStart(int32_t value)
      {
        mIsDirty = true;
        mStart = value;
      }
      int32_t GetEnd() const
      {
        return mEnd;
      }
      void SetEnd(int32_t value)
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
      // return true only if mStart, mEnd, or mDirection have been modified
      bool IsDirty() const
      {
        return mIsDirty;
      }
    private:
      int32_t mStart, mEnd;
      bool mIsDirty = false;
      nsITextControlFrame::SelectionDirection mDirection;
  };

  bool IsSelectionCached() const;
  SelectionProperties& GetSelectionProperties();
  void SetSelectionProperties(SelectionProperties& aProps);
  void WillInitEagerly() { mSelectionRestoreEagerInit = true; }
  bool HasNeverInitializedBefore() const { return !mEverInited; }

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
  nsITextControlElement* const MOZ_NON_OWNING_REF mTextCtrlElement;
  RefPtr<nsTextInputSelectionImpl> mSelCon;
  RefPtr<RestoreSelectionState> mRestoringSelection;
  nsCOMPtr<nsIEditor> mEditor;
  nsCOMPtr<mozilla::dom::Element> mRootNode;
  nsCOMPtr<mozilla::dom::Element> mPlaceholderDiv;
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
