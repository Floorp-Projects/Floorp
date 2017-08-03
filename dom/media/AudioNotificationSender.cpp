/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioNotificationSender.h"
#include "mozilla/ClearOnShutdown.h"    // for ClearOnShutdown
#include "mozilla/dom/ContentParent.h"  // for ContentParent
#include "mozilla/Logging.h"            // for LazyLogModule
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/StaticPtr.h"          // for StaticAutoPtr
#include "nsAppRunner.h"                // for XRE_IsParentProcess
#include "nsTArray.h"                   // for nsTArray
#include <mmdeviceapi.h>                // for IMMNotificationClient interface

static mozilla::LazyLogModule sLogger("AudioNotificationSender");

#undef ANS_LOG
#define ANS_LOG(...) MOZ_LOG(sLogger, mozilla::LogLevel::Debug, (__VA_ARGS__))
#undef ANS_LOGW
#define ANS_LOGW(...) MOZ_LOG(sLogger, mozilla::LogLevel::Warning, (__VA_ARGS__))

namespace mozilla {
namespace audio {

/*
 * A runnable task to notify the audio device-changed event.
 */
class AudioDeviceChangedRunnable final : public Runnable
{
public:
  explicit AudioDeviceChangedRunnable(): Runnable("AudioDeviceChangedRunnable")
  {}

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsTArray<dom::ContentParent*> parents;
    dom::ContentParent::GetAll(parents);
    for (dom::ContentParent* p : parents) {
      Unused << p->SendAudioDefaultDeviceChange();
    }
    return NS_OK;
  }
}; // class AudioDeviceChangedRunnable

/*
 * An observer for receiving audio device events from Windows.
 */
typedef void (* DefaultDeviceChangedCallback)();
class AudioNotification final : public IMMNotificationClient
{
public:
  explicit AudioNotification(DefaultDeviceChangedCallback aCallback)
    : mCallback(aCallback)
    , mRefCt(0)
    , mIsRegistered(false)
  {
    MOZ_COUNT_CTOR(AudioNotification);
    MOZ_ASSERT(mCallback);
    const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
    const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
    HRESULT hr = CoCreateInstance(CLSID_MMDeviceEnumerator,
                                  nullptr,
                                  CLSCTX_INPROC_SERVER,
                                  IID_IMMDeviceEnumerator,
                                  getter_AddRefs(mDeviceEnumerator));

    if (FAILED(hr)) {
      ANS_LOGW("Cannot create an IMMDeviceEnumerator instance.");
      return;
    }

    hr = mDeviceEnumerator->RegisterEndpointNotificationCallback(this);
    if (FAILED(hr)) {
      ANS_LOGW("Cannot register notification callback.");
      return;
    }

    ANS_LOG("Register notification callback successfully.");
    mIsRegistered = true;
  }

  ~AudioNotification()
  {
    MOZ_COUNT_DTOR(AudioNotification);
    // Assert mIsRegistered is true when we have mDeviceEnumerator.
    // Don't care mIsRegistered if there is no mDeviceEnumerator.
    MOZ_ASSERT(!mDeviceEnumerator || mIsRegistered);
    if (!mDeviceEnumerator) {
      ANS_LOG("No device enumerator in use.");
      return;
    }

    HRESULT hr = mDeviceEnumerator->UnregisterEndpointNotificationCallback(this);
    if (FAILED(hr)) {
      // We can't really do anything here, so we just add a log for debugging.
      ANS_LOGW("Unregister notification failed.");
    } else {
      ANS_LOG("Unregister notification callback successfully.");
    }

    mIsRegistered = false;
  }

  // True whenever the notification server is set to report events to this object.
  bool IsRegistered() const
  {
    return mIsRegistered;
  }

  // IMMNotificationClient Implementation
  HRESULT STDMETHODCALLTYPE
  OnDefaultDeviceChanged(EDataFlow aFlow, ERole aRole, LPCWSTR aDeviceId) override
  {
    ANS_LOG("Default device has changed: flow %d, role: %d\n", aFlow, aRole);
    mCallback();
    return S_OK;
  }

  // The remaining methods are not implemented. they simply log when called
  // (if log is enabled), for debugging.
  HRESULT STDMETHODCALLTYPE
  OnDeviceAdded(LPCWSTR aDeviceId) override
  {
    ANS_LOG("Audio device added.");
    return S_OK;
  };

  HRESULT STDMETHODCALLTYPE
  OnDeviceRemoved(LPCWSTR aDeviceId) override
  {
    ANS_LOG("Audio device removed.");
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  OnDeviceStateChanged(LPCWSTR aDeviceId, DWORD aNewState) override
  {
    ANS_LOG("Audio device state changed.");
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  OnPropertyValueChanged(LPCWSTR aDeviceId, const PROPERTYKEY aKey) override
  {
    ANS_LOG("Audio device property value changed.");
    return S_OK;
  }

  // IUnknown Implementation
  ULONG STDMETHODCALLTYPE
  AddRef() override
  {
    return InterlockedIncrement(&mRefCt);
  }

  ULONG STDMETHODCALLTYPE
  Release() override
  {
    ULONG ulRef = InterlockedDecrement(&mRefCt);
    if (0 == ulRef) {
      delete this;
    }
    return ulRef;
  }

  HRESULT STDMETHODCALLTYPE
  QueryInterface(REFIID riid, VOID **ppvInterface) override
  {
    if (__uuidof(IUnknown) == riid) {
      AddRef();
      *ppvInterface = static_cast<IUnknown*>(this);
    } else if (__uuidof(IMMNotificationClient) == riid) {
      AddRef();
      *ppvInterface = static_cast<IMMNotificationClient*>(this);
    } else {
      *ppvInterface = NULL;
      return E_NOINTERFACE;
    }
    return S_OK;
  }

private:
  RefPtr<IMMDeviceEnumerator> mDeviceEnumerator;
  DefaultDeviceChangedCallback mCallback;
  LONG mRefCt;
  bool mIsRegistered;
}; // class AudioNotification

/*
 * A singleton observer for audio device changed events.
 */
static StaticAutoPtr<AudioNotification> sAudioNotification;

/*
 * AudioNotificationSender Implementation
 */
/* static */ nsresult
AudioNotificationSender::Init()
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (!sAudioNotification) {
    sAudioNotification = new AudioNotification(NotifyDefaultDeviceChanged);
    ClearOnShutdown(&sAudioNotification);

    if (!sAudioNotification->IsRegistered()) {
      ANS_LOGW("The notification sender cannot be initialized.");
      return NS_ERROR_FAILURE;
    }
    ANS_LOG("The notification sender is initailized successfully.");
  }

  return NS_OK;
}

/* static */ void
AudioNotificationSender::NotifyDefaultDeviceChanged()
{
  // This is running on the callback thread (from OnDefaultDeviceChanged).
  MOZ_ASSERT(XRE_IsParentProcess());
  ANS_LOG("Notify the default device-changed event.");

  RefPtr<AudioDeviceChangedRunnable> runnable = new AudioDeviceChangedRunnable();
  NS_DispatchToMainThread(runnable);
}

} // namespace audio
} // namespace mozilla
