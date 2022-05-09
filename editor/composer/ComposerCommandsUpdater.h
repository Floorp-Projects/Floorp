/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ComposerCommandsUpdater_h
#define mozilla_ComposerCommandsUpdater_h

#include "nsCOMPtr.h"  // for already_AddRefed, nsCOMPtr
#include "nsCycleCollectionParticipant.h"
#include "nsINamed.h"
#include "nsISupportsImpl.h"  // for NS_DECL_ISUPPORTS
#include "nsITimer.h"         // for NS_DECL_NSITIMERCALLBACK, etc
#include "nscore.h"           // for NS_IMETHOD, nsresult, etc

class nsCommandManager;
class nsIDocShell;
class nsITransaction;
class nsITransactionManager;
class nsPIDOMWindowOuter;

namespace mozilla {

class TransactionManager;

class ComposerCommandsUpdater final : public nsITimerCallback, public nsINamed {
 public:
  ComposerCommandsUpdater();

  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(ComposerCommandsUpdater,
                                           nsITimerCallback)

  // nsITimerCallback
  NS_DECL_NSITIMERCALLBACK

  // nsINamed
  NS_DECL_NSINAMED

  void Init(nsPIDOMWindowOuter& aDOMWindow);

  /**
   * OnSelectionChange() is called when selection is changed in the editor.
   */
  void OnSelectionChange() { PrimeUpdateTimer(); }

  /**
   * OnHTMLEditorCreated() is called when `HTMLEditor` is created and
   * initialized.
   */
  MOZ_CAN_RUN_SCRIPT void OnHTMLEditorCreated() {
    UpdateOneCommand("obs_documentCreated");
  }

  /**
   * OnBeforeHTMLEditorDestroyed() is called when `HTMLEditor` is being
   * destroyed.
   */
  MOZ_CAN_RUN_SCRIPT void OnBeforeHTMLEditorDestroyed() {
    // cancel any outstanding update timer
    if (mUpdateTimer) {
      mUpdateTimer->Cancel();
      mUpdateTimer = nullptr;
    }

    // We can't notify the command manager of this right now; it is too late in
    // some cases and the window is already partially destructed (e.g. JS
    // objects may be gone).
  }

  /**
   * OnHTMLEditorDirtyStateChanged() is called when dirty state of `HTMLEditor`
   * is changed form or to "dirty".
   */
  MOZ_CAN_RUN_SCRIPT void OnHTMLEditorDirtyStateChanged(bool aNowDirty) {
    if (mDirtyState == static_cast<int8_t>(aNowDirty)) {
      return;
    }
    UpdateCommandGroup(CommandGroup::Save);
    UpdateCommandGroup(CommandGroup::Undo);
    mDirtyState = aNowDirty;
  }

  /**
   * The following methods are called when aTransactionManager did
   * `DoTransaction`, `UndoTransaction` or `RedoTransaction` of a transaction
   * instance.
   */
  MOZ_CAN_RUN_SCRIPT void DidDoTransaction(
      TransactionManager& aTransactionManager);
  MOZ_CAN_RUN_SCRIPT void DidUndoTransaction(
      TransactionManager& aTransactionManager);
  MOZ_CAN_RUN_SCRIPT void DidRedoTransaction(
      TransactionManager& aTransactionManager);

 protected:
  virtual ~ComposerCommandsUpdater();

  enum {
    eStateUninitialized = -1,
    eStateOff = 0,
    eStateOn = 1,
  };

  bool SelectionIsCollapsed();
  MOZ_CAN_RUN_SCRIPT nsresult UpdateOneCommand(const char* aCommand);
  enum class CommandGroup {
    Save,
    Style,
    Undo,
  };
  MOZ_CAN_RUN_SCRIPT void UpdateCommandGroup(CommandGroup aCommandGroup);

  nsCommandManager* GetCommandManager();

  nsresult PrimeUpdateTimer();
  void TimerCallback();

  nsCOMPtr<nsITimer> mUpdateTimer;
  nsCOMPtr<nsPIDOMWindowOuter> mDOMWindow;
  nsCOMPtr<nsIDocShell> mDocShell;

  int8_t mDirtyState;
  int8_t mSelectionCollapsed;
  bool mFirstDoOfFirstUndo;
};

}  // namespace mozilla

#endif  // #ifndef mozilla_ComposerCommandsUpdater_h
