/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ComposerCommandsUpdater_h
#define mozilla_ComposerCommandsUpdater_h

#include "nsCOMPtr.h"  // for already_AddRefed, nsCOMPtr
#include "nsCycleCollectionParticipant.h"
#include "nsIDocumentStateListener.h"
#include "nsINamed.h"
#include "nsISupportsImpl.h"         // for NS_DECL_ISUPPORTS
#include "nsITimer.h"                // for NS_DECL_NSITIMERCALLBACK, etc
#include "nsITransactionListener.h"  // for nsITransactionListener
#include "nscore.h"                  // for NS_IMETHOD, nsresult, etc

class nsCommandManager;
class nsIDocShell;
class nsITransaction;
class nsITransactionManager;
class nsPIDOMWindowOuter;

namespace mozilla {

class ComposerCommandsUpdater final : public nsIDocumentStateListener,
                                      public nsITransactionListener,
                                      public nsITimerCallback,
                                      public nsINamed {
 public:
  ComposerCommandsUpdater();

  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(ComposerCommandsUpdater,
                                           nsIDocumentStateListener)

  // nsIDocumentStateListener
  NS_DECL_NSIDOCUMENTSTATELISTENER

  // nsITimerCallback
  NS_DECL_NSITIMERCALLBACK

  // nsINamed
  NS_DECL_NSINAMED

  // nsITransactionListener
  NS_DECL_NSITRANSACTIONLISTENER

  nsresult Init(nsPIDOMWindowOuter* aDOMWindow);

  /**
   * OnSelectionChange() is called when selection is changed in the editor.
   */
  void OnSelectionChange() { PrimeUpdateTimer(); }

 protected:
  virtual ~ComposerCommandsUpdater();

  enum {
    eStateUninitialized = -1,
    eStateOff = 0,
    eStateOn = 1,
  };

  bool SelectionIsCollapsed();
  nsresult UpdateDirtyState(bool aNowDirty);
  nsresult UpdateOneCommand(const char* aCommand);
  nsresult UpdateCommandGroup(const nsAString& aCommandGroup);

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
