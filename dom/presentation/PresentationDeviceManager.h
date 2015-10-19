/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationDeviceManager_h__
#define mozilla_dom_PresentationDeviceManager_h__

#include "nsIObserver.h"
#include "nsIPresentationDevice.h"
#include "nsIPresentationDeviceManager.h"
#include "nsIPresentationDeviceProvider.h"
#include "nsCOMArray.h"
#include "nsWeakReference.h"

namespace mozilla {
namespace dom {

class PresentationDeviceManager final : public nsIPresentationDeviceManager
                                      , public nsIPresentationDeviceListener
                                      , public nsIObserver
                                      , public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONDEVICEMANAGER
  NS_DECL_NSIPRESENTATIONDEVICELISTENER
  NS_DECL_NSIOBSERVER

  PresentationDeviceManager();

private:
  virtual ~PresentationDeviceManager();

  void Init();

  void Shutdown();

  void LoadDeviceProviders();

  void UnloadDeviceProviders();

  void NotifyDeviceChange(nsIPresentationDevice* aDevice,
                          const char16_t* aType);

  nsCOMArray<nsIPresentationDeviceProvider> mProviders;
  nsCOMArray<nsIPresentationDevice> mDevices;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_PresentationDeviceManager_h__ */
