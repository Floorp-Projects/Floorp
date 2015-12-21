/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_devicestorage_DeviceStorageStatics_h
#define mozilla_dom_devicestorage_DeviceStorageStatics_h

#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "nsArrayUtils.h"

class nsString;
class nsDOMDeviceStorage;
class DeviceStorageFile;
#ifdef MOZ_WIDGET_GONK
class nsIVolume;
#endif

namespace mozilla {
namespace dom {
namespace devicestorage {

class DeviceStorageStatics final : public nsIObserver
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static void Initialize();
  static void InitializeDirs();
  static void AddListener(nsDOMDeviceStorage* aListener);
  static void RemoveListener(nsDOMDeviceStorage* aListener);

  static bool LowDiskSpace();
  static bool IsPromptTesting();
  static void GetWritableName(nsString& aName);
  static void SetWritableName(const nsAString& aName);

  static void GetDeviceStorageAreasForIPC(DeviceStorageAreaInfo& aAreaInfo);
  static void RecvDeviceStorageAreasFromParent(const DeviceStorageAreaInfo& aAreaInfo);

  static bool HasOverrideRootDir();
  static already_AddRefed<nsIFile> GetAppsDir();
  static already_AddRefed<nsIFile> GetCrashesDir();
  static already_AddRefed<nsIFile> GetPicturesDir();
  static already_AddRefed<nsIFile> GetVideosDir();
  static already_AddRefed<nsIFile> GetMusicDir();
  static already_AddRefed<nsIFile> GetSdcardDir();

private:
  enum DeviceStorageType {
    TYPE_APPS,
    TYPE_CRASHES,
    TYPE_PICTURES,
    TYPE_VIDEOS,
    TYPE_MUSIC,
    TYPE_SDCARD,
    TYPE_OVERRIDE,
    TYPE_COUNT
  };

  static already_AddRefed<nsIFile> GetDir(DeviceStorageType aType);
  static void GetDirPath(DeviceStorageType aType, nsString& aString);

  DeviceStorageStatics();
  virtual ~DeviceStorageStatics();

  void Init();
  void InitDirs();
  void DumpDirs();
  void Shutdown();
  void Register();
  void Deregister();
  void ResetOverrideRootDir();

  class ListenerWrapper final {
  public:
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ListenerWrapper)

    explicit ListenerWrapper(nsDOMDeviceStorage* aListener);
    bool Equals(nsDOMDeviceStorage* aListener);
    void OnFileWatcherUpdate(const nsCString& aData, DeviceStorageFile* aFile);
    void OnDiskSpaceWatcher(bool aLowDiskSpace);
    void OnWritableNameChanged();
#ifdef MOZ_WIDGET_GONK
    void OnVolumeStateChanged(nsIVolume* aVolume);
#endif

  private:
    virtual ~ListenerWrapper();

    nsWeakPtr mListener;
    nsCOMPtr<nsIThread> mOwningThread;
  };

  nsTArray<RefPtr<ListenerWrapper> > mListeners;
  nsCOMPtr<nsIFile> mDirs[TYPE_COUNT];

  bool mInitialized;
  bool mPromptTesting;
  bool mLowDiskSpace;
  nsString mWritableName;

  static StaticRefPtr<DeviceStorageStatics> sInstance;
  static StaticMutex sMutex;
};

} // namespace devicestorage
} // namespace dom
} // namespace mozilla

#endif
