/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* PluginUtilsWin.cpp - top-level Windows plugin management code */

#include <mmdeviceapi.h>
#include "PluginUtilsWin.h"
#include "PluginModuleParent.h"
#include "mozilla/StaticMutex.h"

namespace mozilla {
namespace plugins {
namespace PluginUtilsWin {

typedef nsTHashtable<nsPtrHashKey<PluginModuleParent>> PluginModuleSet;
StaticMutex sMutex;

class AudioDeviceChangedRunnable : public Runnable
{
public:
  explicit AudioDeviceChangedRunnable(const PluginModuleSet* aAudioNotificationSet,
                                      NPAudioDeviceChangeDetailsIPC aChangeDetails) :
      mChangeDetails(aChangeDetails)
    , mAudioNotificationSet(aAudioNotificationSet)
  {}

  NS_IMETHOD Run() override
  {
    StaticMutexAutoLock lock(sMutex);
    PLUGIN_LOG_DEBUG(("Notifying %d plugins of audio device change.",
                                            mAudioNotificationSet->Count()));

    for (auto iter = mAudioNotificationSet->ConstIter(); !iter.Done(); iter.Next()) {
      PluginModuleParent* pluginModule = iter.Get()->GetKey();
      pluginModule->SendNPP_SetValue_NPNVaudioDeviceChangeDetails(mChangeDetails);
    }
    return NS_OK;
  }

protected:
  NPAudioDeviceChangeDetailsIPC mChangeDetails;
  const PluginModuleSet* mAudioNotificationSet;
};

class AudioNotification : public IMMNotificationClient
{
public:
  AudioNotification() :
      mRefCt(1)
    , mIsRegistered(false)
  {
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
                                  NULL, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&mDeviceEnum));
    if (FAILED(hr)) {
      mDeviceEnum = nullptr;
      return;
    }

    hr = mDeviceEnum->RegisterEndpointNotificationCallback(this);
    if (FAILED(hr)) {
      mDeviceEnum->Release();
      mDeviceEnum = nullptr;
      return;
    }

    mIsRegistered = true;
  }

  ~AudioNotification()
  {
    MOZ_ASSERT(!mIsRegistered,
      "Destroying AudioNotification without first calling Unregister");
    if (mDeviceEnum) {
      mDeviceEnum->Release();
    }
  }

  // IMMNotificationClient Implementation
  HRESULT STDMETHODCALLTYPE
  OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR device_id) override
  {
    NPAudioDeviceChangeDetailsIPC changeDetails;
    changeDetails.flow = (int32_t)flow;
    changeDetails.role = (int32_t)role;
    changeDetails.defaultDevice = std::wstring(device_id);

    // Make sure that plugin is notified on the main thread.
    RefPtr<AudioDeviceChangedRunnable> runnable =
      new AudioDeviceChangedRunnable(&mAudioNotificationSet, changeDetails);
    NS_DispatchToMainThread(runnable);
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  OnDeviceAdded(LPCWSTR device_id) override
  {
    return S_OK;
  };

  HRESULT STDMETHODCALLTYPE
  OnDeviceRemoved(LPCWSTR device_id) override
  {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  OnDeviceStateChanged(LPCWSTR device_id, DWORD new_state) override
  {
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  OnPropertyValueChanged(LPCWSTR device_id, const PROPERTYKEY key) override
  {
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
      *ppvInterface = (IUnknown*)this;
    } else if (__uuidof(IMMNotificationClient) == riid) {
      AddRef();
      *ppvInterface = (IMMNotificationClient*)this;
    } else {
      *ppvInterface = NULL;
      return E_NOINTERFACE;
    }
    return S_OK;
  }

  /*
   * A Valid instance must be Unregistered before Releasing it.
   */
  void Unregister()
  {
    if (mDeviceEnum) {
      mDeviceEnum->UnregisterEndpointNotificationCallback(this);
    }
    mIsRegistered = false;
  }

  /*
   * True whenever the notification server is set to report events to this object.
   */
  bool IsRegistered() {
    return mIsRegistered;
  }

  void AddModule(PluginModuleParent* aModule) {
    StaticMutexAutoLock lock(sMutex);
    mAudioNotificationSet.PutEntry(aModule);
  }

  void RemoveModule(PluginModuleParent* aModule) {
    StaticMutexAutoLock lock(sMutex);
    mAudioNotificationSet.RemoveEntry(aModule);
  }

  /*
   * Are any modules registered for audio notifications?
   */
  bool HasModules() {
    return !mAudioNotificationSet.IsEmpty();
  }

private:
  bool mIsRegistered;   // only used to make sure that Unregister is called before destroying a Valid instance.
  LONG mRefCt;
  IMMDeviceEnumerator* mDeviceEnum;

  // Set of plugin modules that have registered to be notified when the audio device
  // changes.
  PluginModuleSet mAudioNotificationSet;
};  // class AudioNotification

// callback that gets notified of audio device events, or NULL
AudioNotification* sAudioNotification = nullptr;

nsresult
RegisterForAudioDeviceChanges(PluginModuleParent* aModuleParent, bool aShouldRegister)
{
  // Hold the AudioNotification singleton iff there are PluginModuleParents
  // that are subscribed to it.
  if (aShouldRegister) {
    if (!sAudioNotification) {
      // We are registering the first module.  Create the singleton.
      sAudioNotification = new AudioNotification();
      if (!sAudioNotification->IsRegistered()) {
        PLUGIN_LOG_DEBUG(("Registered for plugin audio device notification failed."));
        sAudioNotification->Release();
        sAudioNotification = nullptr;
        return NS_ERROR_FAILURE;
      }
      PLUGIN_LOG_DEBUG(("Registered for plugin audio device notification."));
    }
    sAudioNotification->AddModule(aModuleParent);
  }
  else if (!aShouldRegister && sAudioNotification) {
    sAudioNotification->RemoveModule(aModuleParent);
    if (!sAudioNotification->HasModules()) {
      // We have removed the last module from the notification mechanism
      // so we can destroy the singleton.
      PLUGIN_LOG_DEBUG(("Unregistering for plugin audio device notification."));
      sAudioNotification->Unregister();
      sAudioNotification->Release();
      sAudioNotification = nullptr;
    }
  }
  return NS_OK;
}

}   // namespace PluginUtilsWin
}   // namespace plugins
}   // namespace mozilla
