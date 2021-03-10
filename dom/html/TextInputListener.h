/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TextInputListener_h
#define mozilla_TextInputListener_h

#include "mozilla/WeakPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDOMEventListener.h"
#include "nsStringFwd.h"
#include "nsWeakReference.h"

class nsIFrame;
class nsTextControlFrame;

namespace mozilla {
class TextControlElement;
class TextControlState;

namespace dom {
class Selection;
}  // namespace dom

class TextInputListener final : public nsIDOMEventListener,
                                public nsSupportsWeakReference {
 public:
  explicit TextInputListener(TextControlElement* aTextControlElement);

  void SetFrame(nsIFrame* aTextControlFrame) { mFrame = aTextControlFrame; }
  void SettingValue(bool aValue) { mSettingValue = aValue; }
  void SetValueChanged(bool aSetValueChanged) {
    mSetValueChanged = aSetValueChanged;
  }

  /**
   * aFrame is an optional pointer to our frame, if not passed the method will
   * use mFrame to compute it lazily.
   */
  void HandleValueChanged(nsTextControlFrame* aFrame = nullptr);

  /**
   * OnEditActionHandled() is called when the editor handles each edit action.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
  OnEditActionHandled(TextEditor& aTextEditor);

  /**
   * OnSelectionChange() is called when selection is changed in the editor.
   */
  MOZ_CAN_RUN_SCRIPT
  void OnSelectionChange(dom::Selection& aSelection, int16_t aReason);

  /**
   * Start to listen or end listening to selection change in the editor.
   */
  void StartToListenToSelectionChange() { mListeningToSelectionChange = true; }
  void EndListeningToSelectionChange() { mListeningToSelectionChange = false; }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(TextInputListener,
                                           nsIDOMEventListener)
  NS_DECL_NSIDOMEVENTLISTENER

 protected:
  virtual ~TextInputListener() = default;

  nsresult UpdateTextInputCommands(const nsAString& aCommandsToUpdate,
                                   dom::Selection* aSelection = nullptr,
                                   int16_t aReason = 0);

 protected:
  nsIFrame* mFrame;
  TextControlElement* const mTxtCtrlElement;
  WeakPtr<TextControlState> const mTextControlState;

  bool mSelectionWasCollapsed;

  /**
   * Whether we had undo items or not the last time we got EditAction()
   * notification (when this state changes we update undo and redo menus)
   */
  bool mHadUndoItems;
  /**
   * Whether we had redo items or not the last time we got EditAction()
   * notification (when this state changes we update undo and redo menus)
   */
  bool mHadRedoItems;
  /**
   * Whether we're in the process of a SetValue call, and should therefore
   * refrain from calling OnValueChanged.
   */
  bool mSettingValue;
  /**
   * Whether we are in the process of a SetValue call that doesn't want
   * |SetValueChanged| to be called.
   */
  bool mSetValueChanged;
  /**
   * Whether we're listening to selection change in the editor.
   */
  bool mListeningToSelectionChange;
};

}  // namespace mozilla

#endif  // #ifndef mozilla_TextInputListener_h
