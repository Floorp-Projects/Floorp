/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TextInputListener_h
#define mozilla_TextInputListener_h

#include "nsCycleCollectionParticipant.h"
#include "nsIDOMEventListener.h"
#include "nsISelectionListener.h"
#include "nsStringFwd.h"
#include "nsWeakReference.h"

class nsIFrame;
class nsISelection;
class nsITextControlElement;
class nsTextControlFrame;

namespace mozilla {

class TextInputListener final : public nsISelectionListener
                              , public nsIDOMEventListener
                              , public nsSupportsWeakReference
{
public:
  explicit TextInputListener(nsITextControlElement* aTextControlElement);

  void SetFrame(nsIFrame* aTextControlFrame)
  {
    mFrame = aTextControlFrame;
  }
  void SettingValue(bool aValue)
  {
    mSettingValue = aValue;
  }
  void SetValueChanged(bool aSetValueChanged)
  {
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
  void OnEditActionHandled();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(TextInputListener,
                                           nsISelectionListener)
  NS_DECL_NSISELECTIONLISTENER
  NS_DECL_NSIDOMEVENTLISTENER

protected:
  virtual ~TextInputListener() = default;

  nsresult UpdateTextInputCommands(const nsAString& aCommandsToUpdate,
                                   nsISelection* aSelection = nullptr,
                                   int16_t aReason = 0);

protected:
  nsIFrame* mFrame;
  nsITextControlElement* const mTxtCtrlElement;

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
};

} // namespace mozilla

#endif // #ifndef mozilla_TextInputListener_h
