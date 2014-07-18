/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_power_PowerManager_h
#define mozilla_dom_power_PowerManager_h

#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsIDOMWakeLockListener.h"
#include "nsIDOMWindow.h"
#include "nsWeakReference.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/MozPowerManagerBinding.h"

class nsPIDOMWindow;

namespace mozilla {
class ErrorResult;

namespace dom {

class PowerManager MOZ_FINAL : public nsIDOMMozWakeLockListener
                             , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(PowerManager)
  NS_DECL_NSIDOMMOZWAKELOCKLISTENER

  PowerManager()
  {
    SetIsDOMBinding();
  }

  nsresult Init(nsIDOMWindow *aWindow);
  nsresult Shutdown();

  static already_AddRefed<PowerManager> CreateInstance(nsPIDOMWindow*);

  // WebIDL
  nsIDOMWindow* GetParentObject() const
  {
    return mWindow;
  }
  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;
  void Reboot(ErrorResult& aRv);
  void FactoryReset(mozilla::dom::FactoryResetReason& aReason);
  void PowerOff(ErrorResult& aRv);
  void AddWakeLockListener(nsIDOMMozWakeLockListener* aListener);
  void RemoveWakeLockListener(nsIDOMMozWakeLockListener* aListener);
  void GetWakeLockState(const nsAString& aTopic, nsAString& aState,
                        ErrorResult& aRv);
  bool ScreenEnabled();
  void SetScreenEnabled(bool aEnabled);
  bool KeyLightEnabled();
  void SetKeyLightEnabled(bool aEnabled);
  double ScreenBrightness();
  void SetScreenBrightness(double aBrightness, ErrorResult& aRv);
  bool CpuSleepAllowed();
  void SetCpuSleepAllowed(bool aAllowed);

private:
  nsCOMPtr<nsIDOMWindow> mWindow;
  nsTArray<nsCOMPtr<nsIDOMMozWakeLockListener> > mListeners;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_power_PowerManager_h
