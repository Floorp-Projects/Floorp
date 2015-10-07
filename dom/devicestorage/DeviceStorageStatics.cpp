/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DeviceStorageStatics.h"
#include "mozilla/Preferences.h"
#include "nsDeviceStorage.h"
#include "nsIObserverService.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsISupportsPrimitives.h"
#include "nsPrintfCString.h"

#ifdef MOZ_WIDGET_GONK
#include "nsIVolume.h"
#endif

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#endif

namespace mozilla {
namespace dom {
namespace devicestorage {

static const char* kPrefOverrideRootDir = "device.storage.overrideRootDir";
static const char* kPrefTesting = "device.storage.testing";
static const char* kPrefPromptTesting = "device.storage.prompt.testing";
static const char* kPrefWritableName = "device.storage.writable.name";

static const char* kFileWatcherUpdate = "file-watcher-update";
static const char* kDiskSpaceWatcher = "disk-space-watcher";
static const char* kFileWatcherNotify = "file-watcher-notify";
static const char* kDownloadWatcherNotify = "download-watcher-notify";

StaticRefPtr<DeviceStorageStatics> DeviceStorageStatics::sInstance;
StaticMutex DeviceStorageStatics::sMutex;

NS_IMPL_ISUPPORTS(DeviceStorageStatics,
                  nsIObserver)

/* static */ void
DeviceStorageStatics::Initialize()
{
  MOZ_ASSERT(!sInstance);
  StaticMutexAutoLock lock(sMutex);
  sInstance = new DeviceStorageStatics();
  sInstance->Init();
}

/* static */ void
DeviceStorageStatics::InitializeDirs()
{
  StaticMutexAutoLock lock(sMutex);
  if (NS_WARN_IF(!sInstance)) {
    return;
  }

  /* The actual initialization can only happen on the main thread. This will
     either happen when device storage is first used on the main thread, or
     (in the future) when a worker is created. */
  if (!sInstance->mInitialized && NS_IsMainThread()) {
    sInstance->InitDirs();
    sInstance->mInitialized = true;
  }

  MOZ_ASSERT(sInstance->mInitialized);
}

DeviceStorageStatics::DeviceStorageStatics()
  : mInitialized(false)
  , mPromptTesting(false)
  , mLowDiskSpace(false)
{
  DS_LOG_INFO("");
}

DeviceStorageStatics::~DeviceStorageStatics()
{
  DS_LOG_INFO("");
}

void
DeviceStorageStatics::Init()
{
  MOZ_ASSERT(NS_IsMainThread());
  sMutex.AssertCurrentThreadOwns();
  DS_LOG_INFO("");

  Preferences::AddStrongObserver(this, kPrefTesting);
  Preferences::AddStrongObserver(this, kPrefPromptTesting);
  Preferences::AddStrongObserver(this, kPrefWritableName);

  mWritableName = Preferences::GetString(kPrefWritableName);
  mPromptTesting = Preferences::GetBool(kPrefPromptTesting, false);

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
    obs->AddObserver(this, kFileWatcherNotify, false);
    obs->AddObserver(this, kDownloadWatcherNotify, false);
  }
  DS_LOG_INFO("");
}

void
DeviceStorageStatics::InitDirs()
{
  MOZ_ASSERT(NS_IsMainThread());
  sMutex.AssertCurrentThreadOwns();
  DS_LOG_INFO("");

  nsCOMPtr<nsIProperties> dirService
    = do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
  MOZ_ASSERT(dirService);

#if !defined(MOZ_WIDGET_GONK)

// Keep MOZ_WIDGET_COCOA above XP_UNIX,
// because both are defined in Darwin builds.
#if defined (MOZ_WIDGET_COCOA)
  dirService->Get(NS_OSX_PICTURE_DOCUMENTS_DIR,
                  NS_GET_IID(nsIFile),
                  getter_AddRefs(mDirs[TYPE_PICTURES]));
  dirService->Get(NS_OSX_MOVIE_DOCUMENTS_DIR,
                  NS_GET_IID(nsIFile),
                  getter_AddRefs(mDirs[TYPE_VIDEOS]));
  dirService->Get(NS_OSX_MUSIC_DOCUMENTS_DIR,
                  NS_GET_IID(nsIFile),
                  getter_AddRefs(mDirs[TYPE_MUSIC]));

// Keep MOZ_WIDGET_ANDROID above XP_UNIX,
// because both are defined in Android builds.
#elif defined (MOZ_WIDGET_ANDROID)
  nsAutoString path;
  if (NS_SUCCEEDED(mozilla::AndroidBridge::GetExternalPublicDirectory(
      NS_LITERAL_STRING(DEVICESTORAGE_PICTURES), path))) {
    NS_NewLocalFile(path, /* aFollowLinks */ true,
                    getter_AddRefs(mDirs[TYPE_PICTURES]));
  }
  if (NS_SUCCEEDED(mozilla::AndroidBridge::GetExternalPublicDirectory(
      NS_LITERAL_STRING(DEVICESTORAGE_VIDEOS), path))) {
    NS_NewLocalFile(path, /* aFollowLinks */ true,
                    getter_AddRefs(mDirs[TYPE_VIDEOS]));
  }
  if (NS_SUCCEEDED(mozilla::AndroidBridge::GetExternalPublicDirectory(
      NS_LITERAL_STRING(DEVICESTORAGE_MUSIC), path))) {
    NS_NewLocalFile(path, /* aFollowLinks */ true,
                    getter_AddRefs(mDirs[TYPE_MUSIC]));
  }
  if (NS_SUCCEEDED(mozilla::AndroidBridge::GetExternalPublicDirectory(
      NS_LITERAL_STRING(DEVICESTORAGE_SDCARD), path))) {
    NS_NewLocalFile(path, /* aFollowLinks */ true,
                    getter_AddRefs(mDirs[TYPE_SDCARD]));
  }

#elif defined (XP_UNIX)
  dirService->Get(NS_UNIX_XDG_PICTURES_DIR,
                  NS_GET_IID(nsIFile),
                  getter_AddRefs(mDirs[TYPE_PICTURES]));
  dirService->Get(NS_UNIX_XDG_VIDEOS_DIR,
                  NS_GET_IID(nsIFile),
                  getter_AddRefs(mDirs[TYPE_VIDEOS]));
  dirService->Get(NS_UNIX_XDG_MUSIC_DIR,
                  NS_GET_IID(nsIFile),
                  getter_AddRefs(mDirs[TYPE_MUSIC]));

#elif defined (XP_WIN)
  dirService->Get(NS_WIN_PICTURES_DIR,
                  NS_GET_IID(nsIFile),
                  getter_AddRefs(mDirs[TYPE_PICTURES]));
  dirService->Get(NS_WIN_VIDEOS_DIR,
                  NS_GET_IID(nsIFile),
                  getter_AddRefs(mDirs[TYPE_VIDEOS]));
  dirService->Get(NS_WIN_MUSIC_DIR,
                  NS_GET_IID(nsIFile),
                  getter_AddRefs(mDirs[TYPE_MUSIC]));
#endif

#ifndef MOZ_WIDGET_ANDROID
  // Eventually, on desktop, we want to do something smarter -- for example,
  // detect when an sdcard is inserted, and use that instead of this.
  dirService->Get(NS_APP_USER_PROFILE_50_DIR, NS_GET_IID(nsIFile),
                  getter_AddRefs(mDirs[TYPE_SDCARD]));
  if (mDirs[TYPE_SDCARD]) {
    mDirs[TYPE_SDCARD]->AppendRelativeNativePath(NS_LITERAL_CSTRING("fake-sdcard"));
  }
#endif // !MOZ_WIDGET_ANDROID

  dirService->Get(NS_APP_USER_PROFILE_50_DIR, NS_GET_IID(nsIFile),
                  getter_AddRefs(mDirs[TYPE_APPS]));
  if (mDirs[TYPE_APPS]) {
    mDirs[TYPE_APPS]->AppendRelativeNativePath(NS_LITERAL_CSTRING("webapps"));
  }
#endif // !MOZ_WIDGET_GONK

#ifdef MOZ_WIDGET_GONK
  NS_NewLocalFile(NS_LITERAL_STRING("/data"),
                  false,
                  getter_AddRefs(mDirs[TYPE_APPS]));
#endif

  if (XRE_IsParentProcess()) {
    NS_GetSpecialDirectory("UAppData", getter_AddRefs(mDirs[TYPE_CRASHES]));
    if (mDirs[TYPE_CRASHES]) {
      mDirs[TYPE_CRASHES]->Append(NS_LITERAL_STRING("Crash Reports"));
    }
  }
#ifdef MOZ_WIDGET_GONK
  // NS_GetSpecialDirectory("UAppData") fails in content processes because
  // gAppData from toolkit/xre/nsAppRunner.cpp is not initialized.
  else {
    NS_NewLocalFile(NS_LITERAL_STRING("/data/b2g/mozilla/Crash Reports"),
                                      false,
                                      getter_AddRefs(mDirs[TYPE_CRASHES]));
  }
#endif

  // Directories which don't depend on a volume should be calculated once
  // here. Directories which depend on the root directory of a volume
  // should be calculated in DeviceStorageFile::GetRootDirectoryForType.
  Preferences::AddStrongObserver(this, kPrefOverrideRootDir);
  ResetOverrideRootDir();
}

void
DeviceStorageStatics::DumpDirs()
{
#ifdef DS_LOGGING
  sMutex.AssertCurrentThreadOwns();

  static const char* storageTypes[] = {
    "app",
    "crashes",
    "pictures",
    "videos",
    "music",
    "sdcard",
    "override",
    nullptr
  };

  for (uint32_t i = 0; i < TYPE_COUNT; ++i) {
    MOZ_ASSERT(storageTypes[i]);

    nsString path;
    if (mDirs[i]) {
      mDirs[i]->GetPath(path);
    }
    DS_LOG_INFO("%s: '%s'",
      storageTypes[i], NS_LossyConvertUTF16toASCII(path).get());
  }
#endif
}

void
DeviceStorageStatics::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  sMutex.AssertCurrentThreadOwns();
  DS_LOG_INFO("");

  Preferences::RemoveObserver(this, kPrefOverrideRootDir);
  Preferences::RemoveObserver(this, kPrefTesting);
  Preferences::RemoveObserver(this, kPrefPromptTesting);
  Preferences::RemoveObserver(this, kPrefWritableName);
}

/* static */ already_AddRefed<nsIFile>
DeviceStorageStatics::GetDir(DeviceStorageType aType)
{
  MOZ_ASSERT(aType < TYPE_COUNT);

  StaticMutexAutoLock lock(sMutex);
  if (NS_WARN_IF(!sInstance)) {
    return nullptr;
  }

  nsCOMPtr<nsIFile> file;
  switch (aType) {
    case TYPE_APPS:
    case TYPE_CRASHES:
    case TYPE_OVERRIDE:
      file = sInstance->mDirs[aType];
      return file.forget();
    default:
      break;
  }

  // In testing, we default all device storage types to a temp directory.
  // This is only initialized if the preference device.storage.testing
  // was set to true, or if device.storage.overrideRootDir is set.
  file = sInstance->mDirs[TYPE_OVERRIDE];
  if (!file) {
    file = sInstance->mDirs[aType];
#ifdef MOZ_WIDGET_GONK
    /* We should use volume mount points on B2G. */
    MOZ_ASSERT(!file);
#endif
  }
  return file.forget();
}

/* static */ bool
DeviceStorageStatics::HasOverrideRootDir()
{
  StaticMutexAutoLock lock(sMutex);
  if (NS_WARN_IF(!sInstance)) {
    return false;
  }
  return sInstance->mDirs[TYPE_OVERRIDE];
}

/* static */ already_AddRefed<nsIFile>
DeviceStorageStatics::GetAppsDir()
{
  return GetDir(TYPE_APPS);
}

/* static */ already_AddRefed<nsIFile>
DeviceStorageStatics::GetCrashesDir()
{
  return GetDir(TYPE_CRASHES);
}

/* static */ already_AddRefed<nsIFile>
DeviceStorageStatics::GetPicturesDir()
{
  return GetDir(TYPE_PICTURES);
}

/* static */ already_AddRefed<nsIFile>
DeviceStorageStatics::GetVideosDir()
{
  return GetDir(TYPE_VIDEOS);
}

/* static */ already_AddRefed<nsIFile>
DeviceStorageStatics::GetMusicDir()
{
  return GetDir(TYPE_MUSIC);
}

/* static */ already_AddRefed<nsIFile>
DeviceStorageStatics::GetSdcardDir()
{
  return GetDir(TYPE_SDCARD);
}

/* static */ bool
DeviceStorageStatics::IsPromptTesting()
{
  StaticMutexAutoLock lock(sMutex);
  if (NS_WARN_IF(!sInstance)) {
    return false;
  }
  return sInstance->mPromptTesting;
}

/* static */ bool
DeviceStorageStatics::LowDiskSpace()
{
  StaticMutexAutoLock lock(sMutex);
  if (NS_WARN_IF(!sInstance)) {
    return false;
  }
  return sInstance->mLowDiskSpace;
}

/* static */ void
DeviceStorageStatics::GetWritableName(nsString& aName)
{
  StaticMutexAutoLock lock(sMutex);
  if (NS_WARN_IF(!sInstance)) {
    aName.Truncate();
    return;
  }
  aName = sInstance->mWritableName;
}

/* static */ void
DeviceStorageStatics::SetWritableName(const nsAString& aName)
{
  StaticMutexAutoLock lock(sMutex);
  if (!NS_WARN_IF(!sInstance)) {
    // Update inline although it will be updated again in case
    // another thread comes in checking it before the update takes
    sInstance->mWritableName = aName;
  }

  nsString name;
  name.Assign(aName);
  NS_DispatchToMainThread(NS_NewRunnableFunction([name] () -> void {
    Preferences::SetString(kPrefWritableName, name);
  }));
}

/* static */ void
DeviceStorageStatics::AddListener(nsDOMDeviceStorage* aListener)
{
  DS_LOG_DEBUG("%p", aListener);

  StaticMutexAutoLock lock(sMutex);
  if (NS_WARN_IF(!sInstance)) {
    return;
  }

  MOZ_ASSERT(sInstance->mInitialized);
  if (sInstance->mListeners.IsEmpty()) {
    NS_DispatchToMainThread(
      NS_NewRunnableMethod(sInstance.get(), &DeviceStorageStatics::Register));
  }

  nsRefPtr<ListenerWrapper> wrapper =
    new ListenerWrapper(aListener);
  sInstance->mListeners.AppendElement(wrapper.forget());
}

/* static */ void
DeviceStorageStatics::RemoveListener(nsDOMDeviceStorage* aListener)
{
  DS_LOG_DEBUG("%p", aListener);

  StaticMutexAutoLock lock(sMutex);
  if (!sInstance) {
    return;
  }

  bool removed = false;
  uint32_t i = sInstance->mListeners.Length();
  while (i > 0) {
    --i;
    if (sInstance->mListeners[i]->Equals(aListener)) {
      sInstance->mListeners.RemoveElementAt(i);
      removed = true;
      break;
    }
  }

  if (removed && sInstance->mListeners.IsEmpty()) {
    NS_DispatchToMainThread(
      NS_NewRunnableMethod(sInstance.get(), &DeviceStorageStatics::Deregister));
  }
}

void
DeviceStorageStatics::Register()
{
  MOZ_ASSERT(NS_IsMainThread());
  DS_LOG_INFO("");

  StaticMutexAutoLock lock(sMutex);
  if (NS_WARN_IF(!sInstance)) {
    return;
  }

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, kFileWatcherUpdate, false);
    obs->AddObserver(this, kDiskSpaceWatcher, false);
#ifdef MOZ_WIDGET_GONK
    obs->AddObserver(this, NS_VOLUME_STATE_CHANGED, false);
#endif
  }
}

void
DeviceStorageStatics::Deregister()
{
  MOZ_ASSERT(NS_IsMainThread());
  DS_LOG_INFO("");

  StaticMutexAutoLock lock(sMutex);
  if (!sInstance) {
    return;
  }

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, kFileWatcherUpdate);
    obs->RemoveObserver(this, kDiskSpaceWatcher);
#ifdef MOZ_WIDGET_GONK
    obs->RemoveObserver(this, NS_VOLUME_STATE_CHANGED);
#endif
  }
}

void
DeviceStorageStatics::ResetOverrideRootDir()
{
  MOZ_ASSERT(NS_IsMainThread());
  sMutex.AssertCurrentThreadOwns();
  nsCOMPtr<nsIFile> f;
  DS_LOG_INFO("");

  if (Preferences::GetBool(kPrefTesting, false)) {
    DS_LOG_INFO("temp");
    nsCOMPtr<nsIProperties> dirService
      = do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
    MOZ_ASSERT(dirService);
    dirService->Get(NS_OS_TEMP_DIR, NS_GET_IID(nsIFile), getter_AddRefs(f));
    if (f) {
      f->AppendRelativeNativePath(
        NS_LITERAL_CSTRING("device-storage-testing"));
    }
  } else {
    // For users running on desktop, it's convenient to be able to override
    // all of the directories to point to a single tree, much like what happens
    // on a real device.
    const nsAdoptingString& overrideRootDir =
      mozilla::Preferences::GetString(kPrefOverrideRootDir);
    if (overrideRootDir && !overrideRootDir.IsEmpty()) {
      NS_NewLocalFile(overrideRootDir, false, getter_AddRefs(f));
    }
  }

  if (f) {
    if (XRE_IsParentProcess()) {
      // Only the parent process can create directories. In testing, because
      // the preference is updated after startup, its entirely possible that
      // the preference updated notification will be received by a child
      // prior to the parent.
      nsresult rv = f->Create(nsIFile::DIRECTORY_TYPE, 0777);
      if (NS_FAILED(rv) && rv != NS_ERROR_FILE_ALREADY_EXISTS) {
        nsString path;
        f->GetPath(path);
        nsPrintfCString msg("DeviceStorage: Unable to create directory '%s'",
                            NS_LossyConvertUTF16toASCII(path).get());
        NS_WARNING(msg.get());
      }
    }
    f->Normalize();
  }

  mDirs[TYPE_OVERRIDE] = f.forget();
  DumpDirs();
}

NS_IMETHODIMP
DeviceStorageStatics::Observe(nsISupports* aSubject,
                              const char* aTopic,
                              const char16_t* aData)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    MOZ_ASSERT(aData);

    StaticMutexAutoLock lock(sMutex);
    if (NS_WARN_IF(!sInstance)) {
      return NS_OK;
    }

    nsDependentString name(aData);
    if (name.EqualsASCII(kPrefTesting) ||
        name.EqualsASCII(kPrefOverrideRootDir)) {
      ResetOverrideRootDir();
    } else if(name.EqualsASCII(kPrefPromptTesting)) {
      mPromptTesting = Preferences::GetBool(kPrefPromptTesting, false);
      DS_LOG_INFO("prompt testing %d", mPromptTesting);
    } else if(name.EqualsASCII(kPrefWritableName)) {
      mWritableName = Preferences::GetString(kPrefWritableName);
      uint32_t i = mListeners.Length();
      DS_LOG_INFO("writable name '%s' (%u)",
                  NS_LossyConvertUTF16toASCII(mWritableName).get(), i);
      while (i > 0) {
        --i;
        mListeners[i]->OnWritableNameChanged();
      }
    }
    return NS_OK;
  }

#ifdef MOZ_WIDGET_GONK
  if (!strcmp(aTopic, NS_VOLUME_STATE_CHANGED)) {
    nsCOMPtr<nsIVolume> volume = do_QueryInterface(aSubject);
    if (NS_WARN_IF(!volume)) {
      return NS_OK;
    }

    StaticMutexAutoLock lock(sMutex);
    if (NS_WARN_IF(!sInstance)) {
      return NS_OK;
    }

    uint32_t i = mListeners.Length();
    DS_LOG_INFO("volume updated (%u)", i);
    while (i > 0) {
      --i;
      mListeners[i]->OnVolumeStateChanged(volume);
    }
    return NS_OK;
  }
#endif

  if (!strcmp(aTopic, kFileWatcherUpdate)) {
    DeviceStorageFile* file = static_cast<DeviceStorageFile*>(aSubject);
    if (NS_WARN_IF(!file)) {
      return NS_OK;
    }

    StaticMutexAutoLock lock(sMutex);
    if (NS_WARN_IF(!sInstance)) {
      return NS_OK;
    }

    auto data = NS_ConvertUTF16toUTF8(aData);
    uint32_t i = mListeners.Length();
    DS_LOG_INFO("file updated (%u)", i);
    while (i > 0) {
      --i;
      mListeners[i]->OnFileWatcherUpdate(data, file);
    }
    return NS_OK;
  }

  if (!strcmp(aTopic, kDiskSpaceWatcher)) {
    StaticMutexAutoLock lock(sMutex);
    if (NS_WARN_IF(!sInstance)) {
      return NS_OK;
    }

    // 'disk-space-watcher' notifications are sent when there is a modification
    // of a file in a specific location while a low device storage situation
    // exists or after recovery of a low storage situation. For Firefox OS,
    // these notifications are specific for apps storage.
    if (!NS_strcmp(aData, MOZ_UTF16("full"))) {
      sInstance->mLowDiskSpace = true;
    } else if (!NS_strcmp(aData, MOZ_UTF16("free"))) {
      sInstance->mLowDiskSpace = false;
    } else {
      return NS_OK;
    }


    uint32_t i = mListeners.Length();
    DS_LOG_INFO("disk space %d (%u)", sInstance->mLowDiskSpace, i);
    while (i > 0) {
      --i;
      mListeners[i]->OnDiskSpaceWatcher(sInstance->mLowDiskSpace);
    }
    return NS_OK;
  }

  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    StaticMutexAutoLock lock(sMutex);
    if (NS_WARN_IF(!sInstance)) {
      return NS_OK;
    }

    Shutdown();
    sInstance = nullptr;
    return NS_OK;
  }

  /* Here we convert file-watcher-notify and download-watcher-notify observer
     events to file-watcher-update events.  This is used to be able to
     broadcast events from one child to another child in B2G.  (f.e., if one
     child decides to add a file, we want to be able to able to send a onchange
     notifications to every other child watching that device storage object).*/
  nsRefPtr<DeviceStorageFile> dsf;
  if (!strcmp(aTopic, kDownloadWatcherNotify)) {
    // aSubject will be an nsISupportsString with the native path to the file
    // in question.

    nsCOMPtr<nsISupportsString> supportsString = do_QueryInterface(aSubject);
    if (!supportsString) {
      return NS_OK;
    }
    nsString path;
    nsresult rv = supportsString->GetData(path);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return NS_OK;
    }

    // The downloader uses the sdcard storage type.
    nsString volName;
#ifdef MOZ_WIDGET_GONK
    if (DeviceStorageTypeChecker::IsVolumeBased(NS_LITERAL_STRING(DEVICESTORAGE_SDCARD))) {
      nsCOMPtr<nsIVolumeService> vs = do_GetService(NS_VOLUMESERVICE_CONTRACTID);
      if (NS_WARN_IF(!vs)) {
        return NS_OK;
      }
      nsCOMPtr<nsIVolume> vol;
      rv = vs->GetVolumeByPath(path, getter_AddRefs(vol));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return NS_OK;
      }
      rv = vol->GetName(volName);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return NS_OK;
      }
      nsString mountPoint;
      rv = vol->GetMountPoint(mountPoint);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return NS_OK;
      }
      if (!Substring(path, 0, mountPoint.Length()).Equals(mountPoint)) {
        return NS_OK;
      }
      path = Substring(path, mountPoint.Length() + 1);
    }
#endif
    dsf = new DeviceStorageFile(NS_LITERAL_STRING(DEVICESTORAGE_SDCARD), volName, path);

  } else if (!strcmp(aTopic, kFileWatcherNotify)) {
    dsf = static_cast<DeviceStorageFile*>(aSubject);
  } else {
    DS_LOG_WARN("unhandled topic '%s'", aTopic);
    return NS_OK;
  }

  if (NS_WARN_IF(!dsf || !dsf->mFile)) {
    return NS_OK;
  }

  if (!XRE_IsParentProcess()) {
    // Child process. Forward the notification to the parent.
    ContentChild::GetSingleton()
      ->SendFilePathUpdateNotify(dsf->mStorageType,
                                 dsf->mStorageName,
                                 dsf->mPath,
                                 NS_ConvertUTF16toUTF8(aData));
    return NS_OK;
  }

  // Multiple storage types may match the same files. So walk through each of
  // the storage types, and if the extension matches, tell them about it.
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (DeviceStorageTypeChecker::IsSharedMediaRoot(dsf->mStorageType)) {
    DeviceStorageTypeChecker* typeChecker
      = DeviceStorageTypeChecker::CreateOrGet();
    MOZ_ASSERT(typeChecker);

    static const nsLiteralString kMediaTypes[] = {
      NS_LITERAL_STRING(DEVICESTORAGE_SDCARD),
      NS_LITERAL_STRING(DEVICESTORAGE_PICTURES),
      NS_LITERAL_STRING(DEVICESTORAGE_VIDEOS),
      NS_LITERAL_STRING(DEVICESTORAGE_MUSIC),
    };

    for (size_t i = 0; i < MOZ_ARRAY_LENGTH(kMediaTypes); i++) {
      nsRefPtr<DeviceStorageFile> dsf2;
      if (typeChecker->Check(kMediaTypes[i], dsf->mPath)) {
        if (dsf->mStorageType.Equals(kMediaTypes[i])) {
          dsf2 = dsf;
        } else {
          dsf2 = new DeviceStorageFile(kMediaTypes[i],
                                       dsf->mStorageName, dsf->mPath);
        }
        obs->NotifyObservers(dsf2, kFileWatcherUpdate, aData);
      }
    }
  } else {
    obs->NotifyObservers(dsf, kFileWatcherUpdate, aData);
  }
  return NS_OK;
}

DeviceStorageStatics::ListenerWrapper::ListenerWrapper(nsDOMDeviceStorage* aListener)
  : mListener(do_GetWeakReference(static_cast<DOMEventTargetHelper*>(aListener)))
  , mOwningThread(NS_GetCurrentThread())
{
}

DeviceStorageStatics::ListenerWrapper::~ListenerWrapper()
{
  // Even weak pointers are not thread safe
  NS_ProxyRelease(mOwningThread, mListener);
}

bool
DeviceStorageStatics::ListenerWrapper::Equals(nsDOMDeviceStorage* aListener)
{
  bool current = false;
  mOwningThread->IsOnCurrentThread(&current);
  if (current) {
    // It is only safe to acquire the reference on the owning thread
    nsRefPtr<nsDOMDeviceStorage> listener = do_QueryReferent(mListener);
    return listener.get() == aListener;
  }
  return false;
}

void
DeviceStorageStatics::ListenerWrapper::OnFileWatcherUpdate(const nsCString& aData,
                                                                 DeviceStorageFile* aFile)
{
  nsRefPtr<ListenerWrapper> self = this;
  nsCString data = aData;
  nsRefPtr<DeviceStorageFile> file = aFile;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([self, data, file] () -> void {
    nsRefPtr<nsDOMDeviceStorage> listener = do_QueryReferent(self->mListener);
    if (listener) {
      listener->OnFileWatcherUpdate(data, file);
    }
  });
  mOwningThread->Dispatch(r, NS_DISPATCH_NORMAL);
}

void
DeviceStorageStatics::ListenerWrapper::OnDiskSpaceWatcher(bool aLowDiskSpace)
{
  nsRefPtr<ListenerWrapper> self = this;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([self, aLowDiskSpace] () -> void {
    nsRefPtr<nsDOMDeviceStorage> listener = do_QueryReferent(self->mListener);
    if (listener) {
      listener->OnDiskSpaceWatcher(aLowDiskSpace);
    }
  });
  mOwningThread->Dispatch(r, NS_DISPATCH_NORMAL);
}

void
DeviceStorageStatics::ListenerWrapper::OnWritableNameChanged()
{
  nsRefPtr<ListenerWrapper> self = this;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([self] () -> void {
    nsRefPtr<nsDOMDeviceStorage> listener = do_QueryReferent(self->mListener);
    if (listener) {
      listener->OnWritableNameChanged();
    }
  });
  mOwningThread->Dispatch(r, NS_DISPATCH_NORMAL);
}

#ifdef MOZ_WIDGET_GONK
void
DeviceStorageStatics::ListenerWrapper::OnVolumeStateChanged(nsIVolume* aVolume)
{
  nsRefPtr<ListenerWrapper> self = this;
  nsCOMPtr<nsIVolume> volume = aVolume;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([self, volume] () -> void {
    nsRefPtr<nsDOMDeviceStorage> listener = do_QueryReferent(self->mListener);
    if (listener) {
      listener->OnVolumeStateChanged(volume);
    }
  });
  mOwningThread->Dispatch(r, NS_DISPATCH_NORMAL);
}
#endif

} // namespace devicestorage
} // namespace dom
} // namespace mozilla
