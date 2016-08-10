/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_presentation_provider_DisplayDeviceProvider_h
#define mozilla_dom_presentation_provider_DisplayDeviceProvider_h

#include "mozilla/RefPtr.h"
#include "mozilla/WeakPtr.h"
#include "nsCOMPtr.h"
#include "nsIDOMWindow.h"
#include "nsIDisplayInfo.h"
#include "nsIObserver.h"
#include "nsIPresentationDeviceProvider.h"
#include "nsIPresentationLocalDevice.h"
#include "nsIPresentationControlService.h"
#include "nsITimer.h"
#include "nsIWindowWatcher.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWeakReference.h"

namespace mozilla {
namespace dom {
namespace presentation {

// Consistent definition with the definition in
// widget/gonk/libdisplay/GonkDisplay.h.
enum DisplayType {
    DISPLAY_PRIMARY,
    DISPLAY_EXTERNAL,
    DISPLAY_VIRTUAL,
    NUM_DISPLAY_TYPES
};

class DisplayDeviceProviderWrappedListener;

class DisplayDeviceProvider final : public nsIObserver
                                  , public nsIPresentationDeviceProvider
                                  , public nsIPresentationControlServerListener
                                  , public SupportsWeakPtr<DisplayDeviceProvider>
{
private:
  class HDMIDisplayDevice final : public nsIPresentationLocalDevice
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPRESENTATIONDEVICE
    NS_DECL_NSIPRESENTATIONLOCALDEVICE

    // mScreenId is as same as the definition of display type.
    explicit HDMIDisplayDevice(DisplayDeviceProvider* aProvider)
      : mScreenId(DisplayType::DISPLAY_EXTERNAL)
      , mName("HDMI")
      , mType("external")
      , mWindowId("hdmi")
      , mAddress("127.0.0.1")
      , mProvider(aProvider)
    {}

    nsresult OpenTopLevelWindow();
    nsresult CloseTopLevelWindow();

    const nsCString& Id() const { return mWindowId; }
    const nsCString& Address() const { return mAddress; }

  private:
    virtual ~HDMIDisplayDevice() = default;

    // Due to the limitation of nsWinodw, mScreenId must be an integer.
    // And mScreenId is also align to the display type defined in
    // widget/gonk/libdisplay/GonkDisplay.h.
    // HDMI display is DisplayType::DISPLAY_EXTERNAL.
    uint32_t mScreenId;
    nsCString mName;
    nsCString mType;
    nsCString mWindowId;
    nsCString mAddress;

    nsCOMPtr<mozIDOMWindowProxy> mWindow;
    // weak pointer
    // Provider hold a strong pointer to the device. Use weak pointer to prevent
    // the reference cycle.
    WeakPtr<DisplayDeviceProvider> mProvider;
  };

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIPRESENTATIONDEVICEPROVIDER
  NS_DECL_NSIPRESENTATIONCONTROLSERVERLISTENER
  // For using WeakPtr when MOZ_REFCOUNTED_LEAK_CHECKING defined
  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(DisplayDeviceProvider)

  nsresult Connect(HDMIDisplayDevice* aDevice,
                   nsIPresentationControlChannel** aControlChannel);
private:
  virtual ~DisplayDeviceProvider();

  nsresult Init();
  nsresult Uninit();

  nsresult AddExternalScreen();
  nsresult RemoveExternalScreen();

  nsresult StartTCPService();

  void AbortServerRetry();

  // Now support HDMI display only and there should be only one HDMI display.
  nsCOMPtr<nsIPresentationLocalDevice> mDevice = nullptr;
  // weak pointer
  // PresentationDeviceManager (mDeviceListener) hold strong pointer to
  // DisplayDeviceProvider. Use nsWeakPtr to avoid reference cycle.
  nsWeakPtr mDeviceListener = nullptr;
  nsCOMPtr<nsIPresentationControlService> mPresentationService;
  // Used to prevent reference cycle between DisplayDeviceProvider and
  // TCPPresentationServer.
  RefPtr<DisplayDeviceProviderWrappedListener> mWrappedListener;

  bool mInitialized = false;
  uint16_t mPort;

  bool mIsServerRetrying = false;
  uint32_t mServerRetryMs;
  nsCOMPtr<nsITimer> mServerRetryTimer;
};

} // mozilla
} // dom
} // presentation

#endif // mozilla_dom_presentation_provider_DisplayDeviceProvider_h

