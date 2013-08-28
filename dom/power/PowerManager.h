/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_power_PowerManager_h
#define mozilla_dom_power_PowerManager_h

#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsIDOMPowerManager.h"
#include "nsIDOMWakeLockListener.h"
#include "nsIDOMWindow.h"
#include "nsWeakReference.h"
#include "nsCycleCollectionParticipant.h"

class nsPIDOMWindow;

namespace mozilla {
namespace dom {
namespace power {

class PowerManager
  : public nsIDOMMozPowerManager
  , public nsIDOMMozWakeLockListener
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(PowerManager, nsIDOMMozPowerManager)
  NS_DECL_NSIDOMMOZPOWERMANAGER
  NS_DECL_NSIDOMMOZWAKELOCKLISTENER

  PowerManager() {};
  virtual ~PowerManager() {};

  nsresult Init(nsIDOMWindow *aWindow);
  nsresult Shutdown();

  static bool CheckPermission(nsPIDOMWindow*);

  static already_AddRefed<PowerManager> CreateInstance(nsPIDOMWindow*);

private:

  nsWeakPtr mWindow;
  nsTArray<nsCOMPtr<nsIDOMMozWakeLockListener> > mListeners;
};

} // namespace power
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_power_PowerManager_h
