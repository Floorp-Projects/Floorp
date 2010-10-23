/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=80: */
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
 * The Original Code is Mozilla.org client code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ehsan Akhgari <ehsan@mozilla.com> (Original Author)
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

#ifndef nsTextEditorState_h__
#define nsTextEditorState_h__

#include "nsAutoPtr.h"
#include "nsITextControlElement.h"
#include "nsCycleCollectionParticipant.h"

class nsTextInputListener;
class nsTextControlFrame;
class nsTextInputSelectionImpl;
class nsAnonDivObserver;
class nsISelectionController;
class nsFrameSelection;
class nsIEditor;
class nsITextControlElement;
struct SelectionState;

/**
 * nsTextEditorState is a class which is responsible for managing the state of
 * plaintext controls.  This currently includes the following HTML elements:
 *   <input type=text>
 *   <input type=password>
 *   <textarea>
 * and also XUL controls such as <textbox> which use one of these elements behind
 * the scenes.
 *
 * This class is held as a member of nsHTMLInputElement and nsHTMLTextAreaElement.
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
 *    control.  A mutation observer stored in the mAnonDivObserver has the job of
 *    invalidating this cache when the anonymous contect containing the value is
 *    changed.
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

class nsTextEditorState {
public:
  explicit nsTextEditorState(nsITextControlElement* aOwningElement);
  ~nsTextEditorState();

  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(nsTextEditorState)
  NS_INLINE_DECL_REFCOUNTING(nsTextEditorState)

  nsIEditor* GetEditor();
  nsISelectionController* GetSelectionController() const;
  nsFrameSelection* GetConstFrameSelection();
  nsresult BindToFrame(nsTextControlFrame* aFrame);
  void UnbindFromFrame(nsTextControlFrame* aFrame);
  nsresult PrepareEditor(const nsAString *aValue = nsnull);
  void InitializeKeyboardEventListeners();

  void SetValue(const nsAString& aValue, PRBool aUserInput);
  void GetValue(nsAString& aValue, PRBool aIgnoreWrap) const;
  void EmptyValue() { if (mValue) mValue->Truncate(); }
  PRBool IsEmpty() const { return mValue ? mValue->IsEmpty() : PR_TRUE; }

  nsresult CreatePlaceholderNode();

  nsIContent* GetRootNode() {
    if (!mRootNode)
      CreateRootNode();
    return mRootNode;
  }
  nsIContent* GetPlaceholderNode() {
    return mPlaceholderDiv;
  }

  PRBool IsSingleLineTextControl() const {
    return mTextCtrlElement->IsSingleLineTextControl();
  }
  PRBool IsTextArea() const {
    return mTextCtrlElement->IsTextArea();
  }
  PRBool IsPlainTextControl() const {
    return mTextCtrlElement->IsPlainTextControl();
  }
  PRBool IsPasswordTextControl() const {
    return mTextCtrlElement->IsPasswordTextControl();
  }
  PRInt32 GetCols() {
    return mTextCtrlElement->GetCols();
  }
  PRInt32 GetWrapCols() {
    return mTextCtrlElement->GetWrapCols();
  }
  PRInt32 GetRows() {
    return mTextCtrlElement->GetRows();
  }

  // placeholder methods
  void SetPlaceholderClass(PRBool aVisible, PRBool aNotify);
  void UpdatePlaceholderText(PRBool aNotify); 

  /**
   * Get the maxlength attribute
   * @param aMaxLength the value of the max length attr
   * @returns PR_FALSE if attr not defined
   */
  PRBool GetMaxLength(PRInt32* aMaxLength);

  /* called to free up native keybinding services */
  static NS_HIDDEN_(void) ShutDown();

  void ClearValueCache() { mCachedValue.Truncate(); }

private:
  // not copy constructible
  nsTextEditorState(const nsTextEditorState&);
  // not assignable
  void operator= (const nsTextEditorState&);

  nsresult CreateRootNode();

  void ValueWasChanged(PRBool aNotify);

  void DestroyEditor();
  void Clear();

  class InitializationGuard {
  public:
    explicit InitializationGuard(nsTextEditorState& aState) :
      mState(aState),
      mGuardSet(PR_FALSE)
    {
      if (!mState.mInitializing) {
        mGuardSet = PR_TRUE;
        mState.mInitializing = PR_TRUE;
      }
    }
    ~InitializationGuard() {
      if (mGuardSet) {
        mState.mInitializing = PR_FALSE;
      }
    }
    PRBool IsInitializingRecursively() const {
      return !mGuardSet;
    }
  private:
    nsTextEditorState& mState;
    PRBool mGuardSet;
  };
  friend class InitializationGuard;

  nsITextControlElement* const mTextCtrlElement;
  nsRefPtr<nsTextInputSelectionImpl> mSelCon;
  nsAutoPtr<SelectionState> mSelState;
  nsCOMPtr<nsIEditor> mEditor;
  nsCOMPtr<nsIContent> mRootNode;
  nsCOMPtr<nsIContent> mPlaceholderDiv;
  nsTextControlFrame* mBoundFrame;
  nsTextInputListener* mTextListener;
  nsAutoPtr<nsCString> mValue;
  nsRefPtr<nsAnonDivObserver> mMutationObserver;
  mutable nsString mCachedValue; // Caches non-hard-wrapped value on a multiline control.
  PRPackedBool mEditorInitialized;
  PRPackedBool mInitializing; // Whether we're in the process of initialization
};

#endif
