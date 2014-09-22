/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDeviceStorage.h"

#include "mozilla/Attributes.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/DeviceStorageBinding.h"
#include "mozilla/dom/DeviceStorageChangeEvent.h"
#include "mozilla/dom/DeviceStorageFileSystem.h"
#include "mozilla/dom/devicestorage/PDeviceStorageRequestChild.h"
#include "mozilla/dom/Directory.h"
#include "mozilla/dom/FileSystemUtils.h"
#include "mozilla/dom/ipc/Blob.h"
#include "mozilla/dom/PBrowserChild.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/LazyIdleThread.h"
#include "mozilla/Preferences.h"
#include "mozilla/Scoped.h"
#include "mozilla/Services.h"

#include "nsArrayUtils.h"
#include "nsAutoPtr.h"
#include "nsGlobalWindow.h"
#include "nsServiceManagerUtils.h"
#include "nsIFile.h"
#include "nsIDirectoryEnumerator.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIDOMFile.h"
#include "nsDOMBlobBuilder.h"
#include "nsNetUtil.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIPrincipal.h"
#include "nsJSUtils.h"
#include "nsContentUtils.h"
#include "nsXULAppAPI.h"
#include "DeviceStorageFileDescriptor.h"
#include "DeviceStorageRequestChild.h"
#include "nsCRT.h"
#include "nsIObserverService.h"
#include "nsIMIMEService.h"
#include "nsCExternalHandlerService.h"
#include "nsIPermissionManager.h"
#include "nsIStringBundle.h"
#include "nsIDocument.h"
#include "nsPrintfCString.h"
#include <algorithm>
#include "private/pprio.h"
#include "nsContentPermissionHelper.h"

#include "mozilla/dom/DeviceStorageBinding.h"

// Microsoft's API Name hackery sucks
#undef CreateEvent

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#endif

#ifdef MOZ_WIDGET_GONK
#include "nsIVolume.h"
#include "nsIVolumeService.h"
#endif

#define DEVICESTORAGE_PROPERTIES \
  "chrome://global/content/devicestorage.properties"
#define DEFAULT_THREAD_TIMEOUT_MS 30000

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::devicestorage;
using namespace mozilla::ipc;

#include "nsDirectoryServiceDefs.h"

namespace mozilla {
  MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedPRFileDesc, PRFileDesc, PR_Close);
}

StaticAutoPtr<DeviceStorageUsedSpaceCache>
  DeviceStorageUsedSpaceCache::sDeviceStorageUsedSpaceCache;

DeviceStorageUsedSpaceCache::DeviceStorageUsedSpaceCache()
{
  MOZ_ASSERT(NS_IsMainThread());

  mIOThread = new LazyIdleThread(
    DEFAULT_THREAD_TIMEOUT_MS,
    NS_LITERAL_CSTRING("DeviceStorageUsedSpaceCache I/O"));

}

DeviceStorageUsedSpaceCache::~DeviceStorageUsedSpaceCache()
{
}

DeviceStorageUsedSpaceCache*
DeviceStorageUsedSpaceCache::CreateOrGet()
{
  if (sDeviceStorageUsedSpaceCache) {
    return sDeviceStorageUsedSpaceCache;
  }

  MOZ_ASSERT(NS_IsMainThread());

  sDeviceStorageUsedSpaceCache = new DeviceStorageUsedSpaceCache();
  ClearOnShutdown(&sDeviceStorageUsedSpaceCache);
  return sDeviceStorageUsedSpaceCache;
}

already_AddRefed<DeviceStorageUsedSpaceCache::CacheEntry>
DeviceStorageUsedSpaceCache::GetCacheEntry(const nsAString& aStorageName)
{
  nsTArray<nsRefPtr<CacheEntry>>::size_type numEntries = mCacheEntries.Length();
  nsTArray<nsRefPtr<CacheEntry>>::index_type i;
  for (i = 0; i < numEntries; i++) {
    nsRefPtr<CacheEntry>& cacheEntry = mCacheEntries[i];
    if (cacheEntry->mStorageName.Equals(aStorageName)) {
      nsRefPtr<CacheEntry> addRefedCacheEntry = cacheEntry;
      return addRefedCacheEntry.forget();
    }
  }
  return nullptr;
}

static int64_t
GetFreeBytes(const nsAString& aStorageName)
{
  // This function makes the assumption that the various types
  // are all stored on the same filesystem. So we use pictures.

  nsRefPtr<DeviceStorageFile> dsf(new DeviceStorageFile(NS_LITERAL_STRING(DEVICESTORAGE_PICTURES),
                                                        aStorageName));
  int64_t freeBytes = 0;
  dsf->GetDiskFreeSpace(&freeBytes);
  return freeBytes;
}

nsresult
DeviceStorageUsedSpaceCache::AccumUsedSizes(const nsAString& aStorageName,
                                            uint64_t* aPicturesSoFar,
                                            uint64_t* aVideosSoFar,
                                            uint64_t* aMusicSoFar,
                                            uint64_t* aTotalSoFar)
{
  nsRefPtr<CacheEntry> cacheEntry = GetCacheEntry(aStorageName);
  if (!cacheEntry || cacheEntry->mDirty) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  int64_t freeBytes = GetFreeBytes(cacheEntry->mStorageName);
  if (freeBytes != cacheEntry->mFreeBytes) {
    // Free space changed, so our cached results are no longer valid.
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aPicturesSoFar += cacheEntry->mPicturesUsedSize;
  *aVideosSoFar += cacheEntry->mVideosUsedSize;
  *aMusicSoFar += cacheEntry->mMusicUsedSize;
  *aTotalSoFar += cacheEntry->mTotalUsedSize;

  return NS_OK;
}

void
DeviceStorageUsedSpaceCache::SetUsedSizes(const nsAString& aStorageName,
                                          uint64_t aPictureSize,
                                          uint64_t aVideosSize,
                                          uint64_t aMusicSize,
                                          uint64_t aTotalUsedSize)
{
  nsRefPtr<CacheEntry> cacheEntry = GetCacheEntry(aStorageName);
  if (!cacheEntry) {
    cacheEntry = new CacheEntry;
    cacheEntry->mStorageName = aStorageName;
    mCacheEntries.AppendElement(cacheEntry);
  }
  cacheEntry->mFreeBytes = GetFreeBytes(cacheEntry->mStorageName);

  cacheEntry->mPicturesUsedSize = aPictureSize;
  cacheEntry->mVideosUsedSize = aVideosSize;
  cacheEntry->mMusicUsedSize = aMusicSize;
  cacheEntry->mTotalUsedSize = aTotalUsedSize;
  cacheEntry->mDirty = false;
}

class GlobalDirs
{
private:
  ~GlobalDirs() {}
public:
  NS_INLINE_DECL_REFCOUNTING(GlobalDirs)
#if !defined(MOZ_WIDGET_GONK)
  nsCOMPtr<nsIFile> pictures;
  nsCOMPtr<nsIFile> videos;
  nsCOMPtr<nsIFile> music;
  nsCOMPtr<nsIFile> sdcard;
#endif
  nsCOMPtr<nsIFile> apps;
  nsCOMPtr<nsIFile> crashes;
  nsCOMPtr<nsIFile> overrideRootDir;
};

static StaticRefPtr<GlobalDirs> sDirs;

StaticAutoPtr<DeviceStorageTypeChecker>
  DeviceStorageTypeChecker::sDeviceStorageTypeChecker;

DeviceStorageTypeChecker::DeviceStorageTypeChecker()
{
}

DeviceStorageTypeChecker::~DeviceStorageTypeChecker()
{
}

DeviceStorageTypeChecker*
DeviceStorageTypeChecker::CreateOrGet()
{
  if (sDeviceStorageTypeChecker) {
    return sDeviceStorageTypeChecker;
  }

  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIStringBundleService> stringService
    = mozilla::services::GetStringBundleService();
  if (!stringService) {
    return nullptr;
  }

  nsCOMPtr<nsIStringBundle> filterBundle;
  if (NS_FAILED(stringService->CreateBundle(DEVICESTORAGE_PROPERTIES,
                                            getter_AddRefs(filterBundle)))) {
    return nullptr;
  }

  DeviceStorageTypeChecker* result = new DeviceStorageTypeChecker();
  result->InitFromBundle(filterBundle);

  sDeviceStorageTypeChecker = result;
  ClearOnShutdown(&sDeviceStorageTypeChecker);
  return result;
}

void
DeviceStorageTypeChecker::InitFromBundle(nsIStringBundle* aBundle)
{
  aBundle->GetStringFromName(
    NS_ConvertASCIItoUTF16(DEVICESTORAGE_PICTURES).get(),
    getter_Copies(mPicturesExtensions));
  aBundle->GetStringFromName(
    NS_ConvertASCIItoUTF16(DEVICESTORAGE_MUSIC).get(),
    getter_Copies(mMusicExtensions));
  aBundle->GetStringFromName(
    NS_ConvertASCIItoUTF16(DEVICESTORAGE_VIDEOS).get(),
    getter_Copies(mVideosExtensions));
}


bool
DeviceStorageTypeChecker::Check(const nsAString& aType, nsIDOMBlob* aBlob)
{
  MOZ_ASSERT(aBlob);

  nsString mimeType;
  if (NS_FAILED(aBlob->GetType(mimeType))) {
    return false;
  }

  if (aType.EqualsLiteral(DEVICESTORAGE_PICTURES)) {
    return StringBeginsWith(mimeType, NS_LITERAL_STRING("image/"));
  }

  if (aType.EqualsLiteral(DEVICESTORAGE_VIDEOS)) {
    return StringBeginsWith(mimeType, NS_LITERAL_STRING("video/"));
  }

  if (aType.EqualsLiteral(DEVICESTORAGE_MUSIC)) {
    return StringBeginsWith(mimeType, NS_LITERAL_STRING("audio/"));
  }

  if (aType.EqualsLiteral(DEVICESTORAGE_APPS) ||
      aType.EqualsLiteral(DEVICESTORAGE_SDCARD) ||
      aType.EqualsLiteral(DEVICESTORAGE_CRASHES)) {
    // Apps, crashes and sdcard have no restriction on mime types
    return true;
  }

  return false;
}

bool
DeviceStorageTypeChecker::Check(const nsAString& aType, nsIFile* aFile)
{
  MOZ_ASSERT(aFile);

  if (aType.EqualsLiteral(DEVICESTORAGE_APPS) ||
      aType.EqualsLiteral(DEVICESTORAGE_SDCARD) ||
      aType.EqualsLiteral(DEVICESTORAGE_CRASHES)) {
    // Apps, crashes and sdcard have no restrictions on what file extensions used.
    return true;
  }

  nsString path;
  aFile->GetPath(path);

  int32_t dotIdx = path.RFindChar(char16_t('.'));
  if (dotIdx == kNotFound) {
    return false;
  }

  nsAutoString extensionMatch;
  extensionMatch.Assign('*');
  extensionMatch.Append(Substring(path, dotIdx));
  extensionMatch.Append(';');

  if (aType.EqualsLiteral(DEVICESTORAGE_PICTURES)) {
    return CaseInsensitiveFindInReadable(extensionMatch, mPicturesExtensions);
  }

  if (aType.EqualsLiteral(DEVICESTORAGE_VIDEOS)) {
    return CaseInsensitiveFindInReadable(extensionMatch, mVideosExtensions);
  }

  if (aType.EqualsLiteral(DEVICESTORAGE_MUSIC)) {
    return CaseInsensitiveFindInReadable(extensionMatch, mMusicExtensions);
  }

  return false;
}

void
DeviceStorageTypeChecker::GetTypeFromFile(nsIFile* aFile, nsAString& aType)
{
  MOZ_ASSERT(aFile);

  nsString path;
  aFile->GetPath(path);

  GetTypeFromFileName(path, aType);
}

void
DeviceStorageTypeChecker::GetTypeFromFileName(const nsAString& aFileName,
                                              nsAString& aType)
{
  aType.AssignLiteral(DEVICESTORAGE_SDCARD);

  nsString fileName(aFileName);
  int32_t dotIdx = fileName.RFindChar(char16_t('.'));
  if (dotIdx == kNotFound) {
    return;
  }

  nsAutoString extensionMatch;
  extensionMatch.Assign('*');
  extensionMatch.Append(Substring(aFileName, dotIdx));
  extensionMatch.Append(';');

  if (CaseInsensitiveFindInReadable(extensionMatch, mPicturesExtensions)) {
    aType.AssignLiteral(DEVICESTORAGE_PICTURES);
  }
  else if (CaseInsensitiveFindInReadable(extensionMatch, mVideosExtensions)) {
    aType.AssignLiteral(DEVICESTORAGE_VIDEOS);
  }
  else if (CaseInsensitiveFindInReadable(extensionMatch, mMusicExtensions)) {
    aType.AssignLiteral(DEVICESTORAGE_MUSIC);
  }
}

nsresult
DeviceStorageTypeChecker::GetPermissionForType(const nsAString& aType,
                                               nsACString& aPermissionResult)
{
  if (!aType.EqualsLiteral(DEVICESTORAGE_PICTURES) &&
      !aType.EqualsLiteral(DEVICESTORAGE_VIDEOS) &&
      !aType.EqualsLiteral(DEVICESTORAGE_MUSIC) &&
      !aType.EqualsLiteral(DEVICESTORAGE_APPS) &&
      !aType.EqualsLiteral(DEVICESTORAGE_SDCARD) &&
      !aType.EqualsLiteral(DEVICESTORAGE_CRASHES)) {
    // unknown type
    return NS_ERROR_FAILURE;
  }

  aPermissionResult.AssignLiteral("device-storage:");
  aPermissionResult.Append(NS_ConvertUTF16toUTF8(aType));
  return NS_OK;
}

nsresult
DeviceStorageTypeChecker::GetAccessForRequest(
  const DeviceStorageRequestType aRequestType, nsACString& aAccessResult)
{
  switch(aRequestType) {
    case DEVICE_STORAGE_REQUEST_READ:
    case DEVICE_STORAGE_REQUEST_WATCH:
    case DEVICE_STORAGE_REQUEST_FREE_SPACE:
    case DEVICE_STORAGE_REQUEST_USED_SPACE:
    case DEVICE_STORAGE_REQUEST_AVAILABLE:
    case DEVICE_STORAGE_REQUEST_STATUS:
      aAccessResult.AssignLiteral("read");
      break;
    case DEVICE_STORAGE_REQUEST_WRITE:
    case DEVICE_STORAGE_REQUEST_APPEND:
    case DEVICE_STORAGE_REQUEST_DELETE:
    case DEVICE_STORAGE_REQUEST_FORMAT:
    case DEVICE_STORAGE_REQUEST_MOUNT:
    case DEVICE_STORAGE_REQUEST_UNMOUNT:
      aAccessResult.AssignLiteral("write");
      break;
    case DEVICE_STORAGE_REQUEST_CREATE:
    case DEVICE_STORAGE_REQUEST_CREATEFD:
      aAccessResult.AssignLiteral("create");
      break;
    default:
      aAccessResult.AssignLiteral("undefined");
  }
  return NS_OK;
}

//static
bool
DeviceStorageTypeChecker::IsVolumeBased(const nsAString& aType)
{
#ifdef MOZ_WIDGET_GONK
  // The apps and crashes aren't stored in the same place as the media, so
  // we only ever return a single apps object, and not an array
  // with one per volume (as is the case for the remaining
  // storage types).
  return !aType.EqualsLiteral(DEVICESTORAGE_APPS) &&
         !aType.EqualsLiteral(DEVICESTORAGE_CRASHES);
#else
  return false;
#endif
}

NS_IMPL_ISUPPORTS(FileUpdateDispatcher, nsIObserver)

mozilla::StaticRefPtr<FileUpdateDispatcher> FileUpdateDispatcher::sSingleton;

FileUpdateDispatcher*
FileUpdateDispatcher::GetSingleton()
{
  if (sSingleton) {
    return sSingleton;
  }

  sSingleton = new FileUpdateDispatcher();
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  obs->AddObserver(sSingleton, "file-watcher-notify", false);
  ClearOnShutdown(&sSingleton);

  return sSingleton;
}

NS_IMETHODIMP
FileUpdateDispatcher::Observe(nsISupports *aSubject,
                              const char *aTopic,
                              const char16_t *aData)
{
  if (XRE_GetProcessType() != GeckoProcessType_Default) {

    DeviceStorageFile* file = static_cast<DeviceStorageFile*>(aSubject);
    if (!file || !file->mFile) {
      NS_WARNING("Device storage file looks invalid!");
      return NS_OK;
    }
    ContentChild::GetSingleton()
      ->SendFilePathUpdateNotify(file->mStorageType,
                                 file->mStorageName,
                                 file->mPath,
                                 NS_ConvertUTF16toUTF8(aData));
  } else {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    obs->NotifyObservers(aSubject, "file-watcher-update", aData);
  }
  return NS_OK;
}

class IOEventComplete : public nsRunnable
{
public:
  IOEventComplete(DeviceStorageFile *aFile, const char *aType)
    : mFile(aFile)
    , mType(aType)
  {
  }

  ~IOEventComplete() {}

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    nsString data;
    CopyASCIItoUTF16(mType, data);
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();

    obs->NotifyObservers(mFile, "file-watcher-notify", data.get());

    DeviceStorageUsedSpaceCache* usedSpaceCache
      = DeviceStorageUsedSpaceCache::CreateOrGet();
    MOZ_ASSERT(usedSpaceCache);
    usedSpaceCache->Invalidate(mFile->mStorageName);
    return NS_OK;
  }

private:
  nsRefPtr<DeviceStorageFile> mFile;
  nsCString mType;
};

DeviceStorageFile::DeviceStorageFile(const nsAString& aStorageType,
                                     const nsAString& aStorageName,
                                     const nsAString& aRootDir,
                                     const nsAString& aPath)
  : mStorageType(aStorageType)
  , mStorageName(aStorageName)
  , mRootDir(aRootDir)
  , mPath(aPath)
  , mEditable(false)
  , mLength(UINT64_MAX)
  , mLastModifiedDate(UINT64_MAX)
{
  Init();
  AppendRelativePath(mRootDir);
  if (!mPath.EqualsLiteral("")) {
    AppendRelativePath(mPath);
  }
  NormalizeFilePath();
}

DeviceStorageFile::DeviceStorageFile(const nsAString& aStorageType,
                                     const nsAString& aStorageName,
                                     const nsAString& aPath)
  : mStorageType(aStorageType)
  , mStorageName(aStorageName)
  , mPath(aPath)
  , mEditable(false)
  , mLength(UINT64_MAX)
  , mLastModifiedDate(UINT64_MAX)
{
  Init();
  AppendRelativePath(aPath);
  NormalizeFilePath();
}

DeviceStorageFile::DeviceStorageFile(const nsAString& aStorageType,
                                     const nsAString& aStorageName)
  : mStorageType(aStorageType)
  , mStorageName(aStorageName)
  , mEditable(false)
  , mLength(UINT64_MAX)
  , mLastModifiedDate(UINT64_MAX)
{
  Init();
}

void
DeviceStorageFile::Dump(const char* label)
{
  nsString path;
  if (mFile) {
    mFile->GetPath(path);
  } else {
    path = NS_LITERAL_STRING("(null)");
  }
  const char* ptStr;
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    ptStr = "parent";
  } else {
    ptStr = "child";
  }

  printf_stderr("DSF (%s) %s: mStorageType '%s' mStorageName '%s' "
                "mRootDir '%s' mPath '%s' mFile->GetPath '%s'\n",
                ptStr, label,
                NS_LossyConvertUTF16toASCII(mStorageType).get(),
                NS_LossyConvertUTF16toASCII(mStorageName).get(),
                NS_LossyConvertUTF16toASCII(mRootDir).get(),
                NS_LossyConvertUTF16toASCII(mPath).get(),
                NS_LossyConvertUTF16toASCII(path).get());
}

void
DeviceStorageFile::Init()
{
  DeviceStorageFile::GetRootDirectoryForType(mStorageType,
                                             mStorageName,
                                             getter_AddRefs(mFile));

  DebugOnly<DeviceStorageTypeChecker*> typeChecker
    = DeviceStorageTypeChecker::CreateOrGet();
  MOZ_ASSERT(typeChecker);
}

// The OverrideRootDir is needed to facilitate testing of the
// device.storage.overrideRootDir preference. The preference is normally
// only read once during initialization, but since the test environment has
// no convenient way to restart, we use a pref watcher instead.
class OverrideRootDir MOZ_FINAL : public nsIObserver
{
  ~OverrideRootDir();

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static OverrideRootDir* GetSingleton();
  void Init();
private:
  static mozilla::StaticRefPtr<OverrideRootDir> sSingleton;
};

NS_IMPL_ISUPPORTS(OverrideRootDir, nsIObserver)

mozilla::StaticRefPtr<OverrideRootDir>
  OverrideRootDir::sSingleton;

OverrideRootDir*
OverrideRootDir::GetSingleton()
{
  if (sSingleton) {
    return sSingleton;
  }
  // Preference changes are automatically forwarded from parent to child
  // in ContentParent::Observe, so we'll see the change in both the parent
  // and the child process.

  sSingleton = new OverrideRootDir();
  Preferences::AddStrongObserver(sSingleton, "device.storage.overrideRootDir");
  Preferences::AddStrongObserver(sSingleton, "device.storage.testing");
  ClearOnShutdown(&sSingleton);

  return sSingleton;
}

OverrideRootDir::~OverrideRootDir()
{
  Preferences::RemoveObserver(this, "device.storage.overrideRootDir");
  Preferences::RemoveObserver(this, "device.storage.testing");
}

NS_IMETHODIMP
OverrideRootDir::Observe(nsISupports *aSubject,
                              const char *aTopic,
                              const char16_t *aData)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (sSingleton) {
    sSingleton->Init();
  }
  return NS_OK;
}

void
OverrideRootDir::Init()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!sDirs) {
    return;
  }

  if (mozilla::Preferences::GetBool("device.storage.testing", false)) {
    nsCOMPtr<nsIProperties> dirService
      = do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
    MOZ_ASSERT(dirService);
    dirService->Get(NS_OS_TEMP_DIR, NS_GET_IID(nsIFile),
                    getter_AddRefs(sDirs->overrideRootDir));
    if (sDirs->overrideRootDir) {
      sDirs->overrideRootDir->AppendRelativeNativePath(
        NS_LITERAL_CSTRING("device-storage-testing"));
    }
  } else {
    // For users running on desktop, it's convenient to be able to override
    // all of the directories to point to a single tree, much like what happens
    // on a real device.
    const nsAdoptingString& overrideRootDir =
      mozilla::Preferences::GetString("device.storage.overrideRootDir");
    if (overrideRootDir && overrideRootDir.Length() > 0) {
      NS_NewLocalFile(overrideRootDir, false,
                      getter_AddRefs(sDirs->overrideRootDir));
    } else {
      sDirs->overrideRootDir = nullptr;
    }
  }

  if (sDirs->overrideRootDir) {
    if (XRE_GetProcessType() == GeckoProcessType_Default) {
      // Only the parent process can create directories. In testing, because
      // the preference is updated after startup, its entirely possible that
      // the preference updated notification will be received by a child
      // prior to the parent.
      nsresult rv
        = sDirs->overrideRootDir->Create(nsIFile::DIRECTORY_TYPE, 0777);
      nsString path;
      sDirs->overrideRootDir->GetPath(path);
      if (NS_FAILED(rv) && rv != NS_ERROR_FILE_ALREADY_EXISTS) {
        nsPrintfCString msg("DeviceStorage: Unable to create directory '%s'",
                            NS_LossyConvertUTF16toASCII(path).get());
        NS_WARNING(msg.get());
      }
    }
    sDirs->overrideRootDir->Normalize();
  }
}

// Directories which don't depend on a volume should be calculated once
// here. Directories which depend on the root directory of a volume
// should be calculated in DeviceStorageFile::GetRootDirectoryForType.
static void
InitDirs()
{
  if (sDirs) {
    return;
  }
  MOZ_ASSERT(NS_IsMainThread());
  sDirs = new GlobalDirs;
  ClearOnShutdown(&sDirs);

  nsCOMPtr<nsIProperties> dirService
    = do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
  MOZ_ASSERT(dirService);

#if !defined(MOZ_WIDGET_GONK)

// Keep MOZ_WIDGET_COCOA above XP_UNIX,
// because both are defined in Darwin builds.
#if defined (MOZ_WIDGET_COCOA)
  dirService->Get(NS_OSX_PICTURE_DOCUMENTS_DIR,
                  NS_GET_IID(nsIFile),
                  getter_AddRefs(sDirs->pictures));
  dirService->Get(NS_OSX_MOVIE_DOCUMENTS_DIR,
                  NS_GET_IID(nsIFile),
                  getter_AddRefs(sDirs->videos));
  dirService->Get(NS_OSX_MUSIC_DOCUMENTS_DIR,
                  NS_GET_IID(nsIFile),
                  getter_AddRefs(sDirs->music));

// Keep MOZ_WIDGET_ANDROID above XP_UNIX,
// because both are defined in Android builds.
#elif defined (MOZ_WIDGET_ANDROID)
  nsAutoString path;
  if (NS_SUCCEEDED(mozilla::AndroidBridge::GetExternalPublicDirectory(
      NS_LITERAL_STRING(DEVICESTORAGE_PICTURES), path))) {
    NS_NewLocalFile(path, /* aFollowLinks */ true,
                    getter_AddRefs(sDirs->pictures));
  }
  if (NS_SUCCEEDED(mozilla::AndroidBridge::GetExternalPublicDirectory(
      NS_LITERAL_STRING(DEVICESTORAGE_VIDEOS), path))) {
    NS_NewLocalFile(path, /* aFollowLinks */ true,
                    getter_AddRefs(sDirs->videos));
  }
  if (NS_SUCCEEDED(mozilla::AndroidBridge::GetExternalPublicDirectory(
      NS_LITERAL_STRING(DEVICESTORAGE_MUSIC), path))) {
    NS_NewLocalFile(path, /* aFollowLinks */ true,
                    getter_AddRefs(sDirs->music));
  }
  if (NS_SUCCEEDED(mozilla::AndroidBridge::GetExternalPublicDirectory(
      NS_LITERAL_STRING(DEVICESTORAGE_SDCARD), path))) {
    NS_NewLocalFile(path, /* aFollowLinks */ true,
                    getter_AddRefs(sDirs->sdcard));
  }

#elif defined (XP_UNIX)
  dirService->Get(NS_UNIX_XDG_PICTURES_DIR,
                  NS_GET_IID(nsIFile),
                  getter_AddRefs(sDirs->pictures));
  dirService->Get(NS_UNIX_XDG_VIDEOS_DIR,
                  NS_GET_IID(nsIFile),
                  getter_AddRefs(sDirs->videos));
  dirService->Get(NS_UNIX_XDG_MUSIC_DIR,
                  NS_GET_IID(nsIFile),
                  getter_AddRefs(sDirs->music));

#elif defined (XP_WIN)
  dirService->Get(NS_WIN_PICTURES_DIR,
                  NS_GET_IID(nsIFile),
                  getter_AddRefs(sDirs->pictures));
  dirService->Get(NS_WIN_VIDEOS_DIR,
                  NS_GET_IID(nsIFile),
                  getter_AddRefs(sDirs->videos));
  dirService->Get(NS_WIN_MUSIC_DIR,
                  NS_GET_IID(nsIFile),
                  getter_AddRefs(sDirs->music));
#endif

#ifndef MOZ_WIDGET_ANDROID
  // Eventually, on desktop, we want to do something smarter -- for example,
  // detect when an sdcard is inserted, and use that instead of this.
  dirService->Get(NS_APP_USER_PROFILE_50_DIR, NS_GET_IID(nsIFile),
                  getter_AddRefs(sDirs->sdcard));
  if (sDirs->sdcard) {
    sDirs->sdcard->AppendRelativeNativePath(NS_LITERAL_CSTRING("fake-sdcard"));
  }
#endif // !MOZ_WIDGET_ANDROID
#endif // !MOZ_WIDGET_GONK

#ifdef MOZ_WIDGET_GONK
  NS_NewLocalFile(NS_LITERAL_STRING("/data"),
                  false,
                  getter_AddRefs(sDirs->apps));
#else
  dirService->Get(NS_APP_USER_PROFILE_50_DIR, NS_GET_IID(nsIFile),
                  getter_AddRefs(sDirs->apps));
  if (sDirs->apps) {
    sDirs->apps->AppendRelativeNativePath(NS_LITERAL_CSTRING("webapps"));
  }
#endif

  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    NS_GetSpecialDirectory("UAppData", getter_AddRefs(sDirs->crashes));
    if (sDirs->crashes) {
      sDirs->crashes->Append(NS_LITERAL_STRING("Crash Reports"));
    }
  } else {
    // NS_GetSpecialDirectory("UAppData") fails in content processes because
    // gAppData from toolkit/xre/nsAppRunner.cpp is not initialized.
#ifdef MOZ_WIDGET_GONK
    NS_NewLocalFile(NS_LITERAL_STRING("/data/b2g/mozilla/Crash Reports"),
                                      false,
                                      getter_AddRefs(sDirs->crashes));
#endif
  }

  OverrideRootDir::GetSingleton()->Init();
}

void
DeviceStorageFile::GetFullPath(nsAString &aFullPath)
{
  aFullPath.Truncate();
  if (!mStorageName.EqualsLiteral("")) {
    aFullPath.Append('/');
    aFullPath.Append(mStorageName);
    aFullPath.Append('/');
  }
  if (!mRootDir.EqualsLiteral("")) {
    aFullPath.Append(mRootDir);
    aFullPath.Append('/');
  }
  aFullPath.Append(mPath);
}


// Directories which don't depend on a volume should be calculated once
// in InitDirs. Directories which depend on the root directory of a volume
// should be calculated in this method.
void
DeviceStorageFile::GetRootDirectoryForType(const nsAString& aStorageType,
                                           const nsAString& aStorageName,
                                           nsIFile** aFile)
{
  nsCOMPtr<nsIFile> f;
  *aFile = nullptr;
  bool allowOverride = true;

  InitDirs();

#ifdef MOZ_WIDGET_GONK
  nsString volMountPoint;
  if (DeviceStorageTypeChecker::IsVolumeBased(aStorageType)) {
    nsCOMPtr<nsIVolumeService> vs = do_GetService(NS_VOLUMESERVICE_CONTRACTID);
    NS_ENSURE_TRUE_VOID(vs);
    nsresult rv;
    nsCOMPtr<nsIVolume> vol;
    rv = vs->GetVolumeByName(aStorageName, getter_AddRefs(vol));
    NS_ENSURE_SUCCESS_VOID(rv);
    vol->GetMountPoint(volMountPoint);
  }
#endif

  // Picture directory
  if (aStorageType.EqualsLiteral(DEVICESTORAGE_PICTURES)) {
#ifdef MOZ_WIDGET_GONK
    NS_NewLocalFile(volMountPoint, false, getter_AddRefs(f));
#else
    f = sDirs->pictures;
#endif
  }

  // Video directory
  else if (aStorageType.EqualsLiteral(DEVICESTORAGE_VIDEOS)) {
#ifdef MOZ_WIDGET_GONK
    NS_NewLocalFile(volMountPoint, false, getter_AddRefs(f));
#else
    f = sDirs->videos;
#endif
  }

  // Music directory
  else if (aStorageType.EqualsLiteral(DEVICESTORAGE_MUSIC)) {
#ifdef MOZ_WIDGET_GONK
    NS_NewLocalFile(volMountPoint, false, getter_AddRefs(f));
#else
    f = sDirs->music;
#endif
  }

  // Apps directory
  else if (aStorageType.EqualsLiteral(DEVICESTORAGE_APPS)) {
    f = sDirs->apps;
    allowOverride = false;
  }

   // default SDCard
   else if (aStorageType.EqualsLiteral(DEVICESTORAGE_SDCARD)) {
#ifdef MOZ_WIDGET_GONK
     NS_NewLocalFile(volMountPoint, false, getter_AddRefs(f));
#else
     f = sDirs->sdcard;
#endif
  }

  // crash reports directory.
  else if (aStorageType.EqualsLiteral(DEVICESTORAGE_CRASHES)) {
    f = sDirs->crashes;
    allowOverride = false;
  } else {
    // Not a storage type that we recognize. Return null
    return;
  }

  // In testing, we default all device storage types to a temp directory.
  // sDirs->overrideRootDir will only have been initialized (in InitDirs)
  // if the preference device.storage.testing was set to true, or if
  // device.storage.overrideRootDir is set. We can't test the preferences
  // directly here, since we may not be on the main thread.
  if (allowOverride && sDirs->overrideRootDir) {
    f = sDirs->overrideRootDir;
  }

  if (f) {
    f->Clone(aFile);
  }
}

//static
already_AddRefed<DeviceStorageFile>
DeviceStorageFile::CreateUnique(nsAString& aFileName,
                                uint32_t aFileType,
                                uint32_t aFileAttributes)
{
  DeviceStorageTypeChecker* typeChecker
    = DeviceStorageTypeChecker::CreateOrGet();
  MOZ_ASSERT(typeChecker);

  nsString storageType;
  typeChecker->GetTypeFromFileName(aFileName, storageType);

  nsString storageName;
  nsString storagePath;
  if (!nsDOMDeviceStorage::ParseFullPath(aFileName, storageName, storagePath)) {
    return nullptr;
  }
  if (storageName.IsEmpty()) {
    nsDOMDeviceStorage::GetDefaultStorageName(storageType, storageName);
  }
  nsRefPtr<DeviceStorageFile> dsf =
    new DeviceStorageFile(storageType, storageName, storagePath);
  if (!dsf->mFile) {
    return nullptr;
  }

  nsresult rv = dsf->mFile->CreateUnique(aFileType, aFileAttributes);
  NS_ENSURE_SUCCESS(rv, nullptr);

  // CreateUnique may cause the filename to change. So we need to update mPath
  // to reflect that.
  nsString leafName;
  dsf->mFile->GetLeafName(leafName);

  int32_t lastSlashIndex = dsf->mPath.RFindChar('/');
  if (lastSlashIndex == kNotFound) {
    dsf->mPath.Assign(leafName);
  } else {
    // Include the last '/'
    dsf->mPath = Substring(dsf->mPath, 0, lastSlashIndex + 1);
    dsf->mPath.Append(leafName);
  }

  return dsf.forget();
}

void
DeviceStorageFile::SetPath(const nsAString& aPath) {
  mPath.Assign(aPath);
  NormalizeFilePath();
}

void
DeviceStorageFile::SetEditable(bool aEditable) {
  mEditable = aEditable;
}

// we want to make sure that the names of file can't reach
// outside of the type of storage the user asked for.
bool
DeviceStorageFile::IsSafePath()
{
  return IsSafePath(mRootDir) && IsSafePath(mPath);
}

bool
DeviceStorageFile::IsSafePath(const nsAString& aPath)
{
  nsAString::const_iterator start, end;
  aPath.BeginReading(start);
  aPath.EndReading(end);

  // if the path is a '~' or starts with '~/', return false.
  NS_NAMED_LITERAL_STRING(tilde, "~");
  NS_NAMED_LITERAL_STRING(tildeSlash, "~/");
  if (aPath.Equals(tilde) ||
      StringBeginsWith(aPath, tildeSlash)) {
    NS_WARNING("Path name starts with tilde!");
    return false;
   }
  // split on /.  if any token is "", ., or .., return false.
  NS_ConvertUTF16toUTF8 cname(aPath);
  char* buffer = cname.BeginWriting();
  const char* token;

  while ((token = nsCRT::strtok(buffer, "/", &buffer))) {
    if (PL_strcmp(token, "") == 0 ||
        PL_strcmp(token, ".") == 0 ||
        PL_strcmp(token, "..") == 0 ) {
      return false;
    }
  }
  return true;
}

void
DeviceStorageFile::NormalizeFilePath() {
  FileSystemUtils::LocalPathToNormalizedPath(mPath, mPath);
}

void
DeviceStorageFile::AppendRelativePath(const nsAString& aPath) {
  if (!mFile) {
    return;
  }
  if (!IsSafePath(aPath)) {
    // All of the APIs (in the child) do checks to verify that the path is
    // valid and return PERMISSION_DENIED if a non-safe path is entered.
    // This check is done in the parent and prevents a compromised
    // child from bypassing the check. It shouldn't be possible for this
    // code path to be taken with a non-compromised child.
    NS_WARNING("Unsafe path detected - ignoring");
    NS_WARNING(NS_LossyConvertUTF16toASCII(aPath).get());
    return;
  }
  nsString localPath;
  FileSystemUtils::NormalizedPathToLocalPath(aPath, localPath);
  mFile->AppendRelativePath(localPath);
}

nsresult
DeviceStorageFile::CreateFileDescriptor(FileDescriptor& aFileDescriptor)
{
  ScopedPRFileDesc fd;
  nsresult rv = mFile->OpenNSPRFileDesc(PR_RDWR | PR_CREATE_FILE,
                                        0660, &fd.rwget());
  NS_ENSURE_SUCCESS(rv, rv);

  // NOTE: The FileDescriptor::PlatformHandleType constructor returns a dup of
  //       the file descriptor, so we don't need the original fd after this.
  //       Our scoped file descriptor will automatically close fd.
  aFileDescriptor = FileDescriptor(
    FileDescriptor::PlatformHandleType(PR_FileDesc2NativeHandle(fd)));
  return NS_OK;
}

nsresult
DeviceStorageFile::Write(nsIInputStream* aInputStream)
{
  if (!aInputStream || !mFile) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = mFile->Create(nsIFile::NORMAL_FILE_TYPE, 00600);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIRunnable> iocomplete = new IOEventComplete(this, "created");
  rv = NS_DispatchToMainThread(iocomplete);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIOutputStream> outputStream;
  NS_NewLocalFileOutputStream(getter_AddRefs(outputStream), mFile);

  if (!outputStream) {
    return NS_ERROR_FAILURE;
  }

  return Append(aInputStream, outputStream);
}

nsresult
DeviceStorageFile::Write(InfallibleTArray<uint8_t>& aBits)
{
  if (!mFile) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = mFile->Create(nsIFile::NORMAL_FILE_TYPE, 00600);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIRunnable> iocomplete = new IOEventComplete(this, "created");
  rv = NS_DispatchToMainThread(iocomplete);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIOutputStream> outputStream;
  NS_NewLocalFileOutputStream(getter_AddRefs(outputStream), mFile);

  if (!outputStream) {
    return NS_ERROR_FAILURE;
  }

  uint32_t wrote;
  outputStream->Write((char*) aBits.Elements(), aBits.Length(), &wrote);
  outputStream->Close();

  iocomplete = new IOEventComplete(this, "modified");
  rv = NS_DispatchToMainThread(iocomplete);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (aBits.Length() != wrote) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
DeviceStorageFile::Append(nsIInputStream* aInputStream)
{
  if (!aInputStream || !mFile) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIOutputStream> outputStream;
  NS_NewLocalFileOutputStream(getter_AddRefs(outputStream), mFile, PR_WRONLY | PR_CREATE_FILE | PR_APPEND, -1, 0);

  if (!outputStream) {
    return NS_ERROR_FAILURE;
  }

  return Append(aInputStream, outputStream);
}


nsresult
DeviceStorageFile::Append(nsIInputStream* aInputStream, nsIOutputStream* aOutputStream)
{
  uint64_t bufSize = 0;
  aInputStream->Available(&bufSize);

  nsCOMPtr<nsIOutputStream> bufferedOutputStream;
  nsresult rv = NS_NewBufferedOutputStream(getter_AddRefs(bufferedOutputStream),
                                  aOutputStream,
                                  4096*4);
  NS_ENSURE_SUCCESS(rv, rv);

  while (bufSize) {
    uint32_t wrote;
    rv = bufferedOutputStream->WriteFrom(
      aInputStream,
        static_cast<uint32_t>(std::min<uint64_t>(bufSize, UINT32_MAX)),
        &wrote);
    if (NS_FAILED(rv)) {
      break;
    }
    bufSize -= wrote;
  }

  nsCOMPtr<nsIRunnable> iocomplete = new IOEventComplete(this, "modified");
  rv = NS_DispatchToMainThread(iocomplete);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bufferedOutputStream->Close();
  aOutputStream->Close();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult
DeviceStorageFile::Remove()
{
  MOZ_ASSERT(!NS_IsMainThread());

  if (!mFile) {
    return NS_ERROR_FAILURE;
  }

  bool check;
  nsresult rv = mFile->Exists(&check);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!check) {
    return NS_OK;
  }

  rv = mFile->Remove(true);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIRunnable> iocomplete = new IOEventComplete(this, "deleted");
  return NS_DispatchToMainThread(iocomplete);
}

nsresult
DeviceStorageFile::CalculateMimeType()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoCString mimeType;
  nsCOMPtr<nsIMIMEService> mimeService =
    do_GetService(NS_MIMESERVICE_CONTRACTID);
  if (mimeService) {
    nsresult rv = mimeService->GetTypeFromFile(mFile, mimeType);
    if (NS_FAILED(rv)) {
      mimeType.Truncate();
      return rv;
    }
  }

  mMimeType = NS_ConvertUTF8toUTF16(mimeType);
  return NS_OK;
}

nsresult
DeviceStorageFile::CalculateSizeAndModifiedDate()
{
  MOZ_ASSERT(!NS_IsMainThread());

  int64_t fileSize;
  nsresult rv = mFile->GetFileSize(&fileSize);
  NS_ENSURE_SUCCESS(rv, rv);

  mLength = fileSize;

  PRTime modDate;
  rv = mFile->GetLastModifiedTime(&modDate);
  NS_ENSURE_SUCCESS(rv, rv);

  mLastModifiedDate = modDate;
  return NS_OK;
}

void
DeviceStorageFile::CollectFiles(nsTArray<nsRefPtr<DeviceStorageFile> > &aFiles,
                                PRTime aSince)
{
  nsString fullRootPath;
  mFile->GetPath(fullRootPath);
  collectFilesInternal(aFiles, aSince, fullRootPath);
}

void
DeviceStorageFile::collectFilesInternal(
  nsTArray<nsRefPtr<DeviceStorageFile> > &aFiles,
  PRTime aSince,
  nsAString& aRootPath)
{
  if (!mFile || !IsAvailable()) {
    return;
  }

  nsCOMPtr<nsISimpleEnumerator> e;
  mFile->GetDirectoryEntries(getter_AddRefs(e));

  if (!e) {
    return;
  }

  nsCOMPtr<nsIDirectoryEnumerator> files = do_QueryInterface(e);
  nsCOMPtr<nsIFile> f;

  while (NS_SUCCEEDED(files->GetNextFile(getter_AddRefs(f))) && f) {

    PRTime msecs;
    f->GetLastModifiedTime(&msecs);

    if (msecs < aSince) {
      continue;
    }

    bool isDir;
    f->IsDirectory(&isDir);

    bool isFile;
    f->IsFile(&isFile);

    nsString fullpath;
    nsresult rv = f->GetPath(fullpath);
    if (NS_FAILED(rv)) {
      continue;
    }

    if (!StringBeginsWith(fullpath, aRootPath)) {
      NS_ERROR("collectFiles returned a path that does not belong!");
      continue;
    }

    nsAString::size_type len = aRootPath.Length() + 1; // +1 for the trailing /
    nsDependentSubstring newPath = Substring(fullpath, len);

    if (isDir) {
      DeviceStorageFile dsf(mStorageType, mStorageName, mRootDir, newPath);
      dsf.collectFilesInternal(aFiles, aSince, aRootPath);
    } else if (isFile) {
      nsRefPtr<DeviceStorageFile> dsf =
        new DeviceStorageFile(mStorageType, mStorageName, mRootDir, newPath);
      dsf->CalculateSizeAndModifiedDate();
      aFiles.AppendElement(dsf);
    }
  }
}

void
DeviceStorageFile::AccumDiskUsage(uint64_t* aPicturesSoFar,
                                  uint64_t* aVideosSoFar,
                                  uint64_t* aMusicSoFar,
                                  uint64_t* aTotalSoFar)
{
  if (!IsAvailable()) {
    return;
  }

  uint64_t pictureUsage = 0, videoUsage = 0, musicUsage = 0, totalUsage = 0;

  if (DeviceStorageTypeChecker::IsVolumeBased(mStorageType)) {
    DeviceStorageUsedSpaceCache* usedSpaceCache =
      DeviceStorageUsedSpaceCache::CreateOrGet();
    MOZ_ASSERT(usedSpaceCache);
    nsresult rv = usedSpaceCache->AccumUsedSizes(mStorageName,
                                                 aPicturesSoFar, aVideosSoFar,
                                                 aMusicSoFar, aTotalSoFar);
    if (NS_SUCCEEDED(rv)) {
      return;
    }
    AccumDirectoryUsage(mFile, &pictureUsage, &videoUsage,
                        &musicUsage, &totalUsage);
    usedSpaceCache->SetUsedSizes(mStorageName, pictureUsage, videoUsage,
                                 musicUsage, totalUsage);
  } else {
    AccumDirectoryUsage(mFile, &pictureUsage, &videoUsage,
                        &musicUsage, &totalUsage);
  }

  *aPicturesSoFar += pictureUsage;
  *aVideosSoFar += videoUsage;
  *aMusicSoFar += musicUsage;
  *aTotalSoFar += totalUsage;
}

void
DeviceStorageFile::AccumDirectoryUsage(nsIFile* aFile,
                                       uint64_t* aPicturesSoFar,
                                       uint64_t* aVideosSoFar,
                                       uint64_t* aMusicSoFar,
                                       uint64_t* aTotalSoFar)
{
  if (!aFile) {
    return;
  }

  nsresult rv;
  nsCOMPtr<nsISimpleEnumerator> e;
  rv = aFile->GetDirectoryEntries(getter_AddRefs(e));

  if (NS_FAILED(rv) || !e) {
    return;
  }

  nsCOMPtr<nsIDirectoryEnumerator> files = do_QueryInterface(e);
  MOZ_ASSERT(files);

  nsCOMPtr<nsIFile> f;
  while (NS_SUCCEEDED(files->GetNextFile(getter_AddRefs(f))) && f) {
    bool isDir;
    rv = f->IsDirectory(&isDir);
    if (NS_FAILED(rv)) {
      continue;
    }

    bool isFile;
    rv = f->IsFile(&isFile);
    if (NS_FAILED(rv)) {
      continue;
    }

    bool isLink;
    rv = f->IsSymlink(&isLink);
    if (NS_FAILED(rv)) {
      continue;
    }

    if (isLink) {
      // for now, lets just totally ignore symlinks.
      NS_WARNING("DirectoryDiskUsage ignores symlinks");
    } else if (isDir) {
      AccumDirectoryUsage(f, aPicturesSoFar, aVideosSoFar,
                          aMusicSoFar, aTotalSoFar);
    } else if (isFile) {

      int64_t size;
      rv = f->GetFileSize(&size);
      if (NS_FAILED(rv)) {
        continue;
      }
      DeviceStorageTypeChecker* typeChecker
        = DeviceStorageTypeChecker::CreateOrGet();
      MOZ_ASSERT(typeChecker);
      nsString type;
      typeChecker->GetTypeFromFile(f, type);

      if (type.EqualsLiteral(DEVICESTORAGE_PICTURES)) {
        *aPicturesSoFar += size;
      }
      else if (type.EqualsLiteral(DEVICESTORAGE_VIDEOS)) {
        *aVideosSoFar += size;
      }
      else if (type.EqualsLiteral(DEVICESTORAGE_MUSIC)) {
        *aMusicSoFar += size;
      }
      *aTotalSoFar += size;
    }
  }
}

void
DeviceStorageFile::GetDiskFreeSpace(int64_t* aSoFar)
{
  DeviceStorageTypeChecker* typeChecker
    = DeviceStorageTypeChecker::CreateOrGet();
  if (!typeChecker) {
    return;
  }
  if (!mFile || !IsAvailable()) {
    return;
  }

  int64_t storageAvail = 0;
  nsresult rv = mFile->GetDiskSpaceAvailable(&storageAvail);
  if (NS_SUCCEEDED(rv)) {
    *aSoFar += storageAvail;
  }
}

bool
DeviceStorageFile::IsAvailable()
{
  nsString status;
  GetStatus(status);
  return status.EqualsLiteral("available");
}

void
DeviceStorageFile::DoFormat(nsAString& aStatus)
{
  DeviceStorageTypeChecker* typeChecker
    = DeviceStorageTypeChecker::CreateOrGet();
  if (!typeChecker) {
    return;
  }
  if (!typeChecker->IsVolumeBased(mStorageType)) {
    aStatus.AssignLiteral("notVolume");
    return;
  }
#ifdef MOZ_WIDGET_GONK
  nsCOMPtr<nsIVolumeService> vs = do_GetService(NS_VOLUMESERVICE_CONTRACTID);
  NS_ENSURE_TRUE_VOID(vs);

  nsCOMPtr<nsIVolume> vol;
  nsresult rv = vs->GetVolumeByName(mStorageName, getter_AddRefs(vol));
  NS_ENSURE_SUCCESS_VOID(rv);
  if (!vol) {
    return;
  }

  vol->Format();

  aStatus.AssignLiteral("formatting");
#endif
  return;
}

void
DeviceStorageFile::DoMount(nsAString& aStatus)
{
  DeviceStorageTypeChecker* typeChecker
    = DeviceStorageTypeChecker::CreateOrGet();
  if (!typeChecker) {
    return;
  }
  if (!typeChecker->IsVolumeBased(mStorageType)) {
    aStatus.AssignLiteral("notVolume");
    return;
  }
#ifdef MOZ_WIDGET_GONK
  nsCOMPtr<nsIVolumeService> vs = do_GetService(NS_VOLUMESERVICE_CONTRACTID);
  NS_ENSURE_TRUE_VOID(vs);

  nsCOMPtr<nsIVolume> vol;
  nsresult rv = vs->GetVolumeByName(mStorageName, getter_AddRefs(vol));
  NS_ENSURE_SUCCESS_VOID(rv);
  if (!vol) {
    return;
  }

  vol->Mount();

  aStatus.AssignLiteral("mounting");
#endif
  return;
}

void
DeviceStorageFile::DoUnmount(nsAString& aStatus)
{
  DeviceStorageTypeChecker* typeChecker
    = DeviceStorageTypeChecker::CreateOrGet();
  if (!typeChecker) {
    return;
  }
  if (!typeChecker->IsVolumeBased(mStorageType)) {
    aStatus.AssignLiteral("notVolume");
    return;
  }
#ifdef MOZ_WIDGET_GONK
  nsCOMPtr<nsIVolumeService> vs = do_GetService(NS_VOLUMESERVICE_CONTRACTID);
  NS_ENSURE_TRUE_VOID(vs);

  nsCOMPtr<nsIVolume> vol;
  nsresult rv = vs->GetVolumeByName(mStorageName, getter_AddRefs(vol));
  NS_ENSURE_SUCCESS_VOID(rv);
  if (!vol) {
    return;
  }

  vol->Unmount();

  aStatus.AssignLiteral("unmounting");
#endif
  return;
}

void
DeviceStorageFile::GetStatus(nsAString& aStatus)
{
  aStatus.AssignLiteral("unavailable");

  DeviceStorageTypeChecker* typeChecker
    = DeviceStorageTypeChecker::CreateOrGet();
  if (!typeChecker) {
    return;
  }
  if (!typeChecker->IsVolumeBased(mStorageType)) {
    aStatus.AssignLiteral("available");
    return;
  }

#ifdef MOZ_WIDGET_GONK
  nsCOMPtr<nsIVolumeService> vs = do_GetService(NS_VOLUMESERVICE_CONTRACTID);
  NS_ENSURE_TRUE_VOID(vs);

  nsCOMPtr<nsIVolume> vol;
  nsresult rv = vs->GetVolumeByName(mStorageName, getter_AddRefs(vol));
  NS_ENSURE_SUCCESS_VOID(rv);
  if (!vol) {
    return;
  }
  bool isMediaPresent;
  rv = vol->GetIsMediaPresent(&isMediaPresent);
  NS_ENSURE_SUCCESS_VOID(rv);
  if (!isMediaPresent) {
    return;
  }
  bool isSharing;
  rv = vol->GetIsSharing(&isSharing);
  NS_ENSURE_SUCCESS_VOID(rv);
  if (isSharing) {
    aStatus.AssignLiteral("shared");
    return;
  }
  bool isFormatting;
  rv = vol->GetIsFormatting(&isFormatting);
  NS_ENSURE_SUCCESS_VOID(rv);
  if (isFormatting) {
    aStatus.AssignLiteral("unavailable");
    return;
  }
  bool isUnmounting;
  rv = vol->GetIsUnmounting(&isUnmounting);
  NS_ENSURE_SUCCESS_VOID(rv);
  if (isUnmounting) {
    aStatus.AssignLiteral("unavailable");
    return;
  }
  int32_t volState;
  rv = vol->GetState(&volState);
  NS_ENSURE_SUCCESS_VOID(rv);
  if (volState == nsIVolume::STATE_MOUNTED) {
    aStatus.AssignLiteral("available");
  }
#endif
}

void
DeviceStorageFile::GetStorageStatus(nsAString& aStatus)
{
  aStatus.AssignLiteral("undefined");

  DeviceStorageTypeChecker* typeChecker
    = DeviceStorageTypeChecker::CreateOrGet();
  if (!typeChecker) {
    return;
  }
  if (!typeChecker->IsVolumeBased(mStorageType)) {
    aStatus.AssignLiteral("available");
    return;
  }

#ifdef MOZ_WIDGET_GONK
  nsCOMPtr<nsIVolumeService> vs = do_GetService(NS_VOLUMESERVICE_CONTRACTID);
  NS_ENSURE_TRUE_VOID(vs);

  nsCOMPtr<nsIVolume> vol;
  nsresult rv = vs->GetVolumeByName(mStorageName, getter_AddRefs(vol));
  NS_ENSURE_SUCCESS_VOID(rv);
  if (!vol) {
    return;
  }

  int32_t volState;
  rv = vol->GetState(&volState);
  NS_ENSURE_SUCCESS_VOID(rv);
  aStatus.AssignASCII(mozilla::system::NS_VolumeStateStr(volState));
#endif
}

NS_IMPL_ISUPPORTS0(DeviceStorageFile)

static void
RegisterForSDCardChanges(nsIObserver* aObserver)
{
#ifdef MOZ_WIDGET_GONK
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  obs->AddObserver(aObserver, NS_VOLUME_STATE_CHANGED, false);
#endif
}

static void
UnregisterForSDCardChanges(nsIObserver* aObserver)
{
#ifdef MOZ_WIDGET_GONK
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  obs->RemoveObserver(aObserver, NS_VOLUME_STATE_CHANGED);
#endif
}

void
nsDOMDeviceStorage::SetRootDirectoryForType(const nsAString& aStorageType,
                                            const nsAString& aStorageName)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIFile> f;
  DeviceStorageFile::GetRootDirectoryForType(aStorageType,
                                             aStorageName,
                                             getter_AddRefs(f));
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  obs->AddObserver(this, "file-watcher-update", false);
  obs->AddObserver(this, "disk-space-watcher", false);
  mRootDirectory = f;
  mStorageType = aStorageType;
  mStorageName = aStorageName;
}

JS::Value
InterfaceToJsval(nsPIDOMWindow* aWindow,
                 nsISupports* aObject,
                 const nsIID* aIID)
{
  nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(aWindow);
  if (!sgo) {
    return JS::NullValue();
  }

  JSObject *unrootedScopeObj = sgo->GetGlobalJSObject();
  NS_ENSURE_TRUE(unrootedScopeObj, JS::NullValue());
  JSRuntime *runtime = JS_GetObjectRuntime(unrootedScopeObj);
  JS::Rooted<JS::Value> someJsVal(runtime);
  JS::Rooted<JSObject*> scopeObj(runtime, unrootedScopeObj);
  nsresult rv;

  { // Protect someJsVal from moving GC in ~JSAutoCompartment
    AutoJSContext cx;
    JSAutoCompartment ac(cx, scopeObj);

    rv = nsContentUtils::WrapNative(cx, aObject, aIID, &someJsVal);
  }
  if (NS_FAILED(rv)) {
    return JS::NullValue();
  }

  return someJsVal;
}

JS::Value
nsIFileToJsval(nsPIDOMWindow* aWindow, DeviceStorageFile* aFile)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  if (!aFile) {
    return JSVAL_NULL;
  }

  if (aFile->mEditable) {
    // TODO - needs janv's file handle support.
    return JSVAL_NULL;
  }

  nsString fullPath;
  aFile->GetFullPath(fullPath);

  // This check is useful to know if somewhere the DeviceStorageFile
  // has not been properly set. Mimetype is not checked because it can be
  // empty.
  MOZ_ASSERT(aFile->mLength != UINT64_MAX);
  MOZ_ASSERT(aFile->mLastModifiedDate != UINT64_MAX);

  nsCOMPtr<nsIDOMBlob> blob = new DOMFile(
    new DOMFileImplFile(fullPath, aFile->mMimeType,
                        aFile->mLength, aFile->mFile,
                        aFile->mLastModifiedDate));
  return InterfaceToJsval(aWindow, blob, &NS_GET_IID(nsIDOMBlob));
}

bool
StringToJsval(nsPIDOMWindow* aWindow, nsAString& aString,
              JS::MutableHandle<JS::Value> result)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(aWindow))) {
    return false;
  }
  JSContext* cx = jsapi.cx();

  if (!xpc::StringToJsval(cx, aString, result)) {
    return false;
  }

  return true;
}

class DeviceStorageCursorRequest MOZ_FINAL
  : public nsIContentPermissionRequest
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(DeviceStorageCursorRequest,
                                           nsIContentPermissionRequest)

  NS_FORWARD_NSICONTENTPERMISSIONREQUEST(mCursor->);

  DeviceStorageCursorRequest(nsDOMDeviceStorageCursor* aCursor)
    : mCursor(aCursor) { }

private:
  ~DeviceStorageCursorRequest() {}

  nsRefPtr<nsDOMDeviceStorageCursor> mCursor;
};

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DeviceStorageCursorRequest)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContentPermissionRequest)
  NS_INTERFACE_MAP_ENTRY(nsIContentPermissionRequest)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DeviceStorageCursorRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DeviceStorageCursorRequest)

NS_IMPL_CYCLE_COLLECTION(DeviceStorageCursorRequest,
                         mCursor)


class PostErrorEvent : public nsRunnable
{
public:
  PostErrorEvent(already_AddRefed<DOMRequest> aRequest, const char* aMessage)
    : mRequest(aRequest)
  {
    CopyASCIItoUTF16(aMessage, mError);
  }

  PostErrorEvent(DOMRequest* aRequest, const char* aMessage)
    : mRequest(aRequest)
  {
    CopyASCIItoUTF16(aMessage, mError);
  }

  ~PostErrorEvent() {}

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    if (!mRequest->GetOwner()) {
      return NS_OK;
    }
    mRequest->FireError(mError);
    mRequest = nullptr;
    return NS_OK;
  }

private:
  nsRefPtr<DOMRequest> mRequest;
  nsString mError;
};

ContinueCursorEvent::ContinueCursorEvent(already_AddRefed<DOMRequest> aRequest)
  : mRequest(aRequest)
{
}

ContinueCursorEvent::ContinueCursorEvent(DOMRequest* aRequest)
  : mRequest(aRequest)
{
}

already_AddRefed<DeviceStorageFile>
ContinueCursorEvent::GetNextFile()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsDOMDeviceStorageCursor* cursor
    = static_cast<nsDOMDeviceStorageCursor*>(mRequest.get());
  nsString cursorStorageType;
  cursor->GetStorageType(cursorStorageType);

  DeviceStorageTypeChecker* typeChecker
    = DeviceStorageTypeChecker::CreateOrGet();
  if (!typeChecker) {
    return nullptr;
  }

  while (cursor->mFiles.Length() > 0) {
    nsRefPtr<DeviceStorageFile> file = cursor->mFiles[0];
    cursor->mFiles.RemoveElementAt(0);
    if (!typeChecker->Check(cursorStorageType, file->mFile)) {
      continue;
    }

    file->CalculateMimeType();
    return file.forget();
  }

  return nullptr;
}

ContinueCursorEvent::~ContinueCursorEvent() {}

void
ContinueCursorEvent::Continue()
{
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    DebugOnly<nsresult> rv = NS_DispatchToMainThread(this);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    return;
  }

  nsRefPtr<DeviceStorageFile> file = GetNextFile();

  if (!file) {
    // done with enumeration.
    DebugOnly<nsresult> rv = NS_DispatchToMainThread(this);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    return;
  }

  nsDOMDeviceStorageCursor* cursor
    = static_cast<nsDOMDeviceStorageCursor*>(mRequest.get());
  nsString cursorStorageType;
  cursor->GetStorageType(cursorStorageType);

  DeviceStorageRequestChild* child
    = new DeviceStorageRequestChild(mRequest, file);
  child->SetCallback(cursor);
  DeviceStorageGetParams params(cursorStorageType,
                                file->mStorageName,
                                file->mRootDir,
                                file->mPath);
  ContentChild::GetSingleton()->SendPDeviceStorageRequestConstructor(child,
                                                                     params);
  mRequest = nullptr;
}

NS_IMETHODIMP
ContinueCursorEvent::Run()
{
  nsCOMPtr<nsPIDOMWindow> window = mRequest->GetOwner();
  if (!window) {
    return NS_OK;
  }

  nsRefPtr<DeviceStorageFile> file = GetNextFile();

  nsDOMDeviceStorageCursor* cursor
    = static_cast<nsDOMDeviceStorageCursor*>(mRequest.get());

  AutoJSContext cx;
  JS::Rooted<JS::Value> val(cx, nsIFileToJsval(window, file));

  if (file) {
    cursor->mOkToCallContinue = true;
    cursor->FireSuccess(val);
  } else {
    cursor->FireDone();
  }
  mRequest = nullptr;
  return NS_OK;
}

class InitCursorEvent : public nsRunnable
{
public:
    InitCursorEvent(DOMRequest* aRequest, DeviceStorageFile* aFile)
    : mFile(aFile)
    , mRequest(aRequest)
  {
  }

  ~InitCursorEvent() {}

  NS_IMETHOD Run() {
    MOZ_ASSERT(!NS_IsMainThread());

    if (mFile->mFile) {
      bool check;
      mFile->mFile->IsDirectory(&check);
      if (!check) {
        nsCOMPtr<nsIRunnable> event =
          new PostErrorEvent(mRequest.forget(),
                             POST_ERROR_EVENT_FILE_NOT_ENUMERABLE);
        return NS_DispatchToMainThread(event);
      }
    }

    nsDOMDeviceStorageCursor* cursor
      = static_cast<nsDOMDeviceStorageCursor*>(mRequest.get());
    mFile->CollectFiles(cursor->mFiles, cursor->mSince);

    nsRefPtr<ContinueCursorEvent> event
      = new ContinueCursorEvent(mRequest.forget());
    event->Continue();

    return NS_OK;
  }


private:
  nsRefPtr<DeviceStorageFile> mFile;
  nsRefPtr<DOMRequest> mRequest;
};

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMDeviceStorageCursor)
  NS_INTERFACE_MAP_ENTRY(nsIContentPermissionRequest)
NS_INTERFACE_MAP_END_INHERITING(DOMCursor)

NS_IMPL_ADDREF_INHERITED(nsDOMDeviceStorageCursor, DOMCursor)
NS_IMPL_RELEASE_INHERITED(nsDOMDeviceStorageCursor, DOMCursor)

nsDOMDeviceStorageCursor::nsDOMDeviceStorageCursor(nsPIDOMWindow* aWindow,
                                                   nsIPrincipal* aPrincipal,
                                                   DeviceStorageFile* aFile,
                                                   PRTime aSince)
  : DOMCursor(aWindow, nullptr)
  , mOkToCallContinue(false)
  , mSince(aSince)
  , mFile(aFile)
  , mPrincipal(aPrincipal)
{
}

nsDOMDeviceStorageCursor::~nsDOMDeviceStorageCursor()
{
}

void
nsDOMDeviceStorageCursor::GetStorageType(nsAString & aType)
{
  aType = mFile->mStorageType;
}

NS_IMETHODIMP
nsDOMDeviceStorageCursor::GetTypes(nsIArray** aTypes)
{
  nsCString type;
  nsresult rv =
    DeviceStorageTypeChecker::GetPermissionForType(mFile->mStorageType, type);
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<nsString> emptyOptions;
  return nsContentPermissionUtils::CreatePermissionArray(type,
                                                         NS_LITERAL_CSTRING("read"),
                                                         emptyOptions,
                                                         aTypes);
}

NS_IMETHODIMP
nsDOMDeviceStorageCursor::GetPrincipal(nsIPrincipal * *aRequestingPrincipal)
{
  NS_IF_ADDREF(*aRequestingPrincipal = mPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceStorageCursor::GetWindow(nsIDOMWindow * *aRequestingWindow)
{
  NS_IF_ADDREF(*aRequestingWindow = GetOwner());
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceStorageCursor::GetElement(nsIDOMElement * *aRequestingElement)
{
  *aRequestingElement = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceStorageCursor::Cancel()
{
  nsCOMPtr<nsIRunnable> event
    = new PostErrorEvent(this, POST_ERROR_EVENT_PERMISSION_DENIED);
  return NS_DispatchToMainThread(event);
}

NS_IMETHODIMP
nsDOMDeviceStorageCursor::Allow(JS::HandleValue aChoices)
{
  MOZ_ASSERT(aChoices.isUndefined());

  if (!mFile->IsSafePath()) {
    nsCOMPtr<nsIRunnable> r
      = new PostErrorEvent(this, POST_ERROR_EVENT_PERMISSION_DENIED);
    return NS_DispatchToMainThread(r);
  }

  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    PDeviceStorageRequestChild* child
      = new DeviceStorageRequestChild(this, mFile);
    DeviceStorageEnumerationParams params(mFile->mStorageType,
                                          mFile->mStorageName,
                                          mFile->mRootDir,
                                          mSince);
    ContentChild::GetSingleton()->SendPDeviceStorageRequestConstructor(child,
                                                                       params);
    return NS_OK;
  }

  nsCOMPtr<nsIEventTarget> target
    = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  MOZ_ASSERT(target);

  nsCOMPtr<nsIRunnable> event = new InitCursorEvent(this, mFile);
  target->Dispatch(event, NS_DISPATCH_NORMAL);
  return NS_OK;
}

void
nsDOMDeviceStorageCursor::Continue(ErrorResult& aRv)
{
  if (!mOkToCallContinue) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  if (mResult != JSVAL_VOID) {
    // We call onsuccess multiple times. Clear the last
    // result.
    mResult = JSVAL_VOID;
    mDone = false;
  }

  nsRefPtr<ContinueCursorEvent> event = new ContinueCursorEvent(this);
  event->Continue();

  mOkToCallContinue = false;
}

void
nsDOMDeviceStorageCursor::RequestComplete()
{
  MOZ_ASSERT(!mOkToCallContinue);
  mOkToCallContinue = true;
}

class PostAvailableResultEvent : public nsRunnable
{
public:
  PostAvailableResultEvent(DeviceStorageFile *aFile, DOMRequest* aRequest)
    : mFile(aFile)
    , mRequest(aRequest)
  {
    MOZ_ASSERT(mRequest);
  }

  ~PostAvailableResultEvent() {}

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsPIDOMWindow> window = mRequest->GetOwner();
    if (!window) {
      return NS_OK;
    }

    nsString state = NS_LITERAL_STRING("unavailable");
    if (mFile) {
      mFile->GetStatus(state);
    }

    AutoJSContext cx;
    JS::Rooted<JS::Value> result(cx);
    StringToJsval(window, state, &result);
    mRequest->FireSuccess(result);
    mRequest = nullptr;
    return NS_OK;
  }

private:
  nsRefPtr<DeviceStorageFile> mFile;
  nsRefPtr<DOMRequest> mRequest;
};

class PostStatusResultEvent : public nsRunnable
{
public:
  PostStatusResultEvent(DeviceStorageFile *aFile, DOMRequest* aRequest)
    : mFile(aFile)
    , mRequest(aRequest)
  {
    MOZ_ASSERT(mRequest);
  }

  ~PostStatusResultEvent() {}

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsPIDOMWindow> window = mRequest->GetOwner();
    if (!window) {
      return NS_OK;
    }

    nsString state = NS_LITERAL_STRING("undefined");
    if (mFile) {
      mFile->GetStorageStatus(state);
    }

    AutoJSContext cx;
    JS::Rooted<JS::Value> result(cx);
    StringToJsval(window, state, &result);
    mRequest->FireSuccess(result);
    mRequest = nullptr;
    return NS_OK;
  }

private:
  nsRefPtr<DeviceStorageFile> mFile;
  nsRefPtr<DOMRequest> mRequest;
};

class PostFormatResultEvent : public nsRunnable
{
public:
  PostFormatResultEvent(DeviceStorageFile *aFile, DOMRequest* aRequest)
    : mFile(aFile)
    , mRequest(aRequest)
  {
    MOZ_ASSERT(mRequest);
  }

  ~PostFormatResultEvent() {}

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsPIDOMWindow> window = mRequest->GetOwner();
    if (!window) {
      return NS_OK;
    }

    nsString state = NS_LITERAL_STRING("unavailable");
    if (mFile) {
      mFile->DoFormat(state);
    }

    AutoJSContext cx;
    JS::Rooted<JS::Value> result(cx);
    StringToJsval(window, state, &result);
    mRequest->FireSuccess(result);
    mRequest = nullptr;
    return NS_OK;
  }

private:
  nsRefPtr<DeviceStorageFile> mFile;
  nsRefPtr<DOMRequest> mRequest;
};

class PostMountResultEvent : public nsRunnable
{
public:
  PostMountResultEvent(DeviceStorageFile *aFile, DOMRequest* aRequest)
    : mFile(aFile)
    , mRequest(aRequest)
  {
    MOZ_ASSERT(mRequest);
  }

  ~PostMountResultEvent() {}

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsPIDOMWindow> window = mRequest->GetOwner();
    if (!window) {
      return NS_OK;
    }

    nsString state = NS_LITERAL_STRING("unavailable");
    if (mFile) {
      mFile->DoMount(state);
    }

    AutoJSContext cx;
    JS::Rooted<JS::Value> result(cx);
    StringToJsval(window, state, &result);
    mRequest->FireSuccess(result);
    mRequest = nullptr;
    return NS_OK;
  }

private:
  nsRefPtr<DeviceStorageFile> mFile;
  nsRefPtr<DOMRequest> mRequest;
};

class PostUnmountResultEvent : public nsRunnable
{
public:
  PostUnmountResultEvent(DeviceStorageFile *aFile, DOMRequest* aRequest)
    : mFile(aFile)
    , mRequest(aRequest)
  {
    MOZ_ASSERT(mRequest);
  }

  ~PostUnmountResultEvent() {}

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsPIDOMWindow> window = mRequest->GetOwner();
    if (!window) {
      return NS_OK;
    }

    nsString state = NS_LITERAL_STRING("unavailable");
    if (mFile) {
      mFile->DoUnmount(state);
    }

    AutoJSContext cx;
    JS::Rooted<JS::Value> result(cx);
    StringToJsval(window, state, &result);
    mRequest->FireSuccess(result);
    mRequest = nullptr;
    return NS_OK;
  }

private:
  nsRefPtr<DeviceStorageFile> mFile;
  nsRefPtr<DOMRequest> mRequest;
};

class PostResultEvent : public nsRunnable
{
public:
  PostResultEvent(already_AddRefed<DOMRequest> aRequest,
                  DeviceStorageFile* aFile)
    : mFile(aFile)
    , mRequest(aRequest)
  {
    MOZ_ASSERT(mRequest);
  }

  PostResultEvent(already_AddRefed<DOMRequest> aRequest,
                  const nsAString & aPath)
    : mPath(aPath)
    , mRequest(aRequest)
  {
    MOZ_ASSERT(mRequest);
  }

  PostResultEvent(already_AddRefed<DOMRequest> aRequest,
                  const uint64_t aValue)
    : mValue(aValue)
    , mRequest(aRequest)
  {
    MOZ_ASSERT(mRequest);
  }

  ~PostResultEvent() {}

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsPIDOMWindow> window = mRequest->GetOwner();
    if (!window) {
      return NS_OK;
    }

    AutoJSContext cx;
    JS::Rooted<JS::Value> result(cx, JSVAL_NULL);

    if (mFile) {
      result = nsIFileToJsval(window, mFile);
    } else if (mPath.Length()) {
      StringToJsval(window, mPath, &result);
    }
    else {
      result = JS_NumberValue(double(mValue));
    }

    mRequest->FireSuccess(result);
    mRequest = nullptr;
    return NS_OK;
  }

private:
  nsRefPtr<DeviceStorageFile> mFile;
  nsString mPath;
  uint64_t mValue;
  nsRefPtr<DOMRequest> mRequest;
};

class CreateFdEvent : public nsRunnable
{
public:
  CreateFdEvent(DeviceStorageFileDescriptor* aDSFileDescriptor,
                already_AddRefed<DOMRequest> aRequest)
    : mDSFileDescriptor(aDSFileDescriptor)
    , mRequest(aRequest)
  {
    MOZ_ASSERT(mDSFileDescriptor);
    MOZ_ASSERT(mRequest);
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    DeviceStorageFile* dsFile = mDSFileDescriptor->mDSFile;
    MOZ_ASSERT(dsFile);

    nsString fullPath;
    dsFile->GetFullPath(fullPath);
    MOZ_ASSERT(!fullPath.IsEmpty());

    bool check = false;
    dsFile->mFile->Exists(&check);
    if (check) {
      nsCOMPtr<nsIRunnable> event =
        new PostErrorEvent(mRequest.forget(), POST_ERROR_EVENT_FILE_EXISTS);
      return NS_DispatchToMainThread(event);
    }

    nsresult rv = dsFile->CreateFileDescriptor(mDSFileDescriptor->mFileDescriptor);

    if (NS_FAILED(rv)) {
      dsFile->mFile->Remove(false);

      nsCOMPtr<nsIRunnable> event =
        new PostErrorEvent(mRequest.forget(), POST_ERROR_EVENT_UNKNOWN);
      return NS_DispatchToMainThread(event);
    }

    nsCOMPtr<nsIRunnable> event =
      new PostResultEvent(mRequest.forget(), fullPath);
    return NS_DispatchToMainThread(event);
  }

private:
  nsRefPtr<DeviceStorageFileDescriptor> mDSFileDescriptor;
  nsRefPtr<DOMRequest> mRequest;
};

class WriteFileEvent : public nsRunnable
{
public:
  WriteFileEvent(DOMFileImpl* aBlobImpl,
                 DeviceStorageFile *aFile,
                 already_AddRefed<DOMRequest> aRequest,
                 int32_t aRequestType)
    : mBlobImpl(aBlobImpl)
    , mFile(aFile)
    , mRequest(aRequest)
    , mRequestType(aRequestType)
  {
    MOZ_ASSERT(mFile);
    MOZ_ASSERT(mRequest);
    MOZ_ASSERT(mRequestType);
  }

  ~WriteFileEvent() {}

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    nsCOMPtr<nsIInputStream> stream;
    mBlobImpl->GetInternalStream(getter_AddRefs(stream));

    bool check = false;
    mFile->mFile->Exists(&check);
    nsresult rv;

    if (mRequestType == DEVICE_STORAGE_REQUEST_APPEND) {
      if (!check) {
        nsCOMPtr<nsIRunnable> event =
          new PostErrorEvent(mRequest.forget(), POST_ERROR_EVENT_FILE_DOES_NOT_EXIST);
        return NS_DispatchToMainThread(event);
      }
      rv = mFile->Append(stream);
    }
    else if (mRequestType == DEVICE_STORAGE_REQUEST_CREATE) {
      if (check) {
        nsCOMPtr<nsIRunnable> event =
          new PostErrorEvent(mRequest.forget(), POST_ERROR_EVENT_FILE_EXISTS);
        return NS_DispatchToMainThread(event);
      }
      rv = mFile->Write(stream);
      if (NS_FAILED(rv)) {
        mFile->mFile->Remove(false);
      }
    } else {
      nsCOMPtr<nsIRunnable> event =
        new PostErrorEvent(mRequest.forget(), POST_ERROR_EVENT_UNKNOWN);
      return NS_DispatchToMainThread(event);
    }

    if (NS_FAILED(rv)) {
      nsCOMPtr<nsIRunnable> event =
        new PostErrorEvent(mRequest.forget(), POST_ERROR_EVENT_UNKNOWN);
      return NS_DispatchToMainThread(event);
    }

    nsString fullPath;
    mFile->GetFullPath(fullPath);
    nsCOMPtr<nsIRunnable> event =
      new PostResultEvent(mRequest.forget(), fullPath);
    return NS_DispatchToMainThread(event);
  }

private:
  nsRefPtr<DOMFileImpl> mBlobImpl;
  nsRefPtr<DeviceStorageFile> mFile;
  nsRefPtr<DOMRequest> mRequest;
  int32_t mRequestType;
};


class ReadFileEvent : public nsRunnable
{
public:
  ReadFileEvent(DeviceStorageFile* aFile,
                already_AddRefed<DOMRequest> aRequest)
    : mFile(aFile)
    , mRequest(aRequest)
  {
    MOZ_ASSERT(mFile);
    MOZ_ASSERT(mRequest);
    mFile->CalculateMimeType();
  }

  ~ReadFileEvent() {}

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    nsCOMPtr<nsIRunnable> r;
    if (!mFile->mEditable) {
      bool check = false;
      mFile->mFile->Exists(&check);
      if (!check) {
        r = new PostErrorEvent(mRequest.forget(),
                               POST_ERROR_EVENT_FILE_DOES_NOT_EXIST);
      }
    }

    if (!r) {
      nsresult rv = mFile->CalculateSizeAndModifiedDate();
      if (NS_FAILED(rv)) {
        r = new PostErrorEvent(mRequest.forget(), POST_ERROR_EVENT_UNKNOWN);
      }
    }

    if (!r) {
      r = new PostResultEvent(mRequest.forget(), mFile);
    }
    return NS_DispatchToMainThread(r);
  }

private:
  nsRefPtr<DeviceStorageFile> mFile;
  nsRefPtr<DOMRequest> mRequest;
};

class DeleteFileEvent : public nsRunnable
{
public:
  DeleteFileEvent(DeviceStorageFile* aFile,
                  already_AddRefed<DOMRequest> aRequest)
    : mFile(aFile)
    , mRequest(aRequest)
  {
    MOZ_ASSERT(mFile);
    MOZ_ASSERT(mRequest);
  }

  ~DeleteFileEvent() {}

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());
    mFile->Remove();

    nsCOMPtr<nsIRunnable> r;
    bool check = false;
    mFile->mFile->Exists(&check);
    if (check) {
      r = new PostErrorEvent(mRequest.forget(),
                             POST_ERROR_EVENT_FILE_DOES_NOT_EXIST);
    }
    else {
      nsString fullPath;
      mFile->GetFullPath(fullPath);
      r = new PostResultEvent(mRequest.forget(), fullPath);
    }
    return NS_DispatchToMainThread(r);
  }

private:
  nsRefPtr<DeviceStorageFile> mFile;
  nsRefPtr<DOMRequest> mRequest;
};

class UsedSpaceFileEvent : public nsRunnable
{
public:
  UsedSpaceFileEvent(DeviceStorageFile* aFile,
                     already_AddRefed<DOMRequest> aRequest)
    : mFile(aFile)
    , mRequest(aRequest)
  {
    MOZ_ASSERT(mFile);
    MOZ_ASSERT(mRequest);
  }

  ~UsedSpaceFileEvent() {}

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    uint64_t picturesUsage = 0, videosUsage = 0, musicUsage = 0, totalUsage = 0;
    mFile->AccumDiskUsage(&picturesUsage, &videosUsage,
                          &musicUsage, &totalUsage);
    nsCOMPtr<nsIRunnable> r;
    if (mFile->mStorageType.EqualsLiteral(DEVICESTORAGE_PICTURES)) {
      r = new PostResultEvent(mRequest.forget(), picturesUsage);
    }
    else if (mFile->mStorageType.EqualsLiteral(DEVICESTORAGE_VIDEOS)) {
      r = new PostResultEvent(mRequest.forget(), videosUsage);
    }
    else if (mFile->mStorageType.EqualsLiteral(DEVICESTORAGE_MUSIC)) {
      r = new PostResultEvent(mRequest.forget(), musicUsage);
    } else {
      r = new PostResultEvent(mRequest.forget(), totalUsage);
    }
    return NS_DispatchToMainThread(r);
  }

private:
  nsRefPtr<DeviceStorageFile> mFile;
  nsRefPtr<DOMRequest> mRequest;
};

class FreeSpaceFileEvent : public nsRunnable
{
public:
  FreeSpaceFileEvent(DeviceStorageFile* aFile,
                     already_AddRefed<DOMRequest> aRequest)
    : mFile(aFile)
    , mRequest(aRequest)
  {
    MOZ_ASSERT(mFile);
    MOZ_ASSERT(mRequest);
  }

  ~FreeSpaceFileEvent() {}

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    int64_t freeSpace = 0;
    if (mFile) {
      mFile->GetDiskFreeSpace(&freeSpace);
    }

    nsCOMPtr<nsIRunnable> r;
    r = new PostResultEvent(mRequest.forget(),
                            static_cast<uint64_t>(freeSpace));
    return NS_DispatchToMainThread(r);
  }

private:
  nsRefPtr<DeviceStorageFile> mFile;
  nsRefPtr<DOMRequest> mRequest;
};

class DeviceStorageRequest MOZ_FINAL
  : public nsIContentPermissionRequest
  , public nsIRunnable
{
public:

  DeviceStorageRequest(const DeviceStorageRequestType aRequestType,
                       nsPIDOMWindow* aWindow,
                       nsIPrincipal* aPrincipal,
                       DeviceStorageFile* aFile,
                       DOMRequest* aRequest,
                       nsDOMDeviceStorage* aDeviceStorage)
    : mRequestType(aRequestType)
    , mWindow(aWindow)
    , mPrincipal(aPrincipal)
    , mFile(aFile)
    , mRequest(aRequest)
    , mDeviceStorage(aDeviceStorage)
  {
    MOZ_ASSERT(mWindow);
    MOZ_ASSERT(mPrincipal);
    MOZ_ASSERT(mFile);
    MOZ_ASSERT(mRequest);
    MOZ_ASSERT(mDeviceStorage);
  }

  DeviceStorageRequest(const DeviceStorageRequestType aRequestType,
                       nsPIDOMWindow* aWindow,
                       nsIPrincipal* aPrincipal,
                       DeviceStorageFile* aFile,
                       DOMRequest* aRequest,
                       nsIDOMBlob* aBlob = nullptr)
    : mRequestType(aRequestType)
    , mWindow(aWindow)
    , mPrincipal(aPrincipal)
    , mFile(aFile)
    , mRequest(aRequest)
    , mBlob(aBlob)
  {
    MOZ_ASSERT(mWindow);
    MOZ_ASSERT(mPrincipal);
    MOZ_ASSERT(mFile);
    MOZ_ASSERT(mRequest);
  }

  DeviceStorageRequest(const DeviceStorageRequestType aRequestType,
                       nsPIDOMWindow* aWindow,
                       nsIPrincipal* aPrincipal,
                       DeviceStorageFile* aFile,
                       DOMRequest* aRequest,
                       DeviceStorageFileDescriptor* aDSFileDescriptor)
    : mRequestType(aRequestType)
    , mWindow(aWindow)
    , mPrincipal(aPrincipal)
    , mFile(aFile)
    , mRequest(aRequest)
    , mDSFileDescriptor(aDSFileDescriptor)
  {
    MOZ_ASSERT(mRequestType == DEVICE_STORAGE_REQUEST_CREATEFD);
    MOZ_ASSERT(mWindow);
    MOZ_ASSERT(mPrincipal);
    MOZ_ASSERT(mFile);
    MOZ_ASSERT(mRequest);
    MOZ_ASSERT(mDSFileDescriptor);
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(DeviceStorageRequest,
                                           nsIContentPermissionRequest)

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (mozilla::Preferences::GetBool("device.storage.prompt.testing", false)) {
      Allow(JS::UndefinedHandleValue);
      return NS_OK;
    }

    return nsContentPermissionUtils::AskPermission(this, mWindow);
  }

  NS_IMETHODIMP GetTypes(nsIArray** aTypes)
  {
    nsCString type;
    nsresult rv =
      DeviceStorageTypeChecker::GetPermissionForType(mFile->mStorageType, type);
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsCString access;
    rv = DeviceStorageTypeChecker::GetAccessForRequest(
      DeviceStorageRequestType(mRequestType), access);
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsTArray<nsString> emptyOptions;
    return nsContentPermissionUtils::CreatePermissionArray(type, access, emptyOptions, aTypes);
  }

  NS_IMETHOD GetPrincipal(nsIPrincipal * *aRequestingPrincipal)
  {
    NS_IF_ADDREF(*aRequestingPrincipal = mPrincipal);
    return NS_OK;
  }

  NS_IMETHOD GetWindow(nsIDOMWindow * *aRequestingWindow)
  {
    NS_IF_ADDREF(*aRequestingWindow = mWindow);
    return NS_OK;
  }

  NS_IMETHOD GetElement(nsIDOMElement * *aRequestingElement)
  {
    *aRequestingElement = nullptr;
    return NS_OK;
  }

  NS_IMETHOD Cancel()
  {
    nsCOMPtr<nsIRunnable> event
      = new PostErrorEvent(mRequest.forget(),
                           POST_ERROR_EVENT_PERMISSION_DENIED);
    return NS_DispatchToMainThread(event);
  }

  NS_IMETHOD Allow(JS::HandleValue aChoices)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aChoices.isUndefined());

    if (!mRequest) {
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIRunnable> r;

    switch(mRequestType) {
      case DEVICE_STORAGE_REQUEST_CREATEFD:
      {
        if (!mFile->mFile) {
          return NS_ERROR_FAILURE;
        }

        DeviceStorageTypeChecker* typeChecker
          = DeviceStorageTypeChecker::CreateOrGet();
        if (!typeChecker) {
          return NS_OK;
        }

        if (!typeChecker->Check(mFile->mStorageType, mFile->mFile)) {
          r = new PostErrorEvent(mRequest.forget(),
                                 POST_ERROR_EVENT_ILLEGAL_TYPE);
          return NS_DispatchToCurrentThread(r);
        }

        if (XRE_GetProcessType() != GeckoProcessType_Default) {

          DeviceStorageCreateFdParams params;
          params.type() = mFile->mStorageType;
          params.storageName() = mFile->mStorageName;
          params.relpath() = mFile->mPath;

          mFile->Dump("DeviceStorageCreateFdParams");

          PDeviceStorageRequestChild* child
            = new DeviceStorageRequestChild(mRequest, mFile,
                                            mDSFileDescriptor.get());
          ContentChild::GetSingleton()
            ->SendPDeviceStorageRequestConstructor(child, params);
          return NS_OK;
        }
        mDSFileDescriptor->mDSFile = mFile;
        r = new CreateFdEvent(mDSFileDescriptor.get(), mRequest.forget());
        break;
      }

      case DEVICE_STORAGE_REQUEST_CREATE:
      {
        if (!mBlob || !mFile->mFile) {
          return NS_ERROR_FAILURE;
        }

        DeviceStorageTypeChecker* typeChecker
          = DeviceStorageTypeChecker::CreateOrGet();
        if (!typeChecker) {
          return NS_OK;
        }

        if (!typeChecker->Check(mFile->mStorageType, mFile->mFile) ||
            !typeChecker->Check(mFile->mStorageType, mBlob)) {
          r = new PostErrorEvent(mRequest.forget(),
                                 POST_ERROR_EVENT_ILLEGAL_TYPE);
          return NS_DispatchToCurrentThread(r);
        }

        if (XRE_GetProcessType() != GeckoProcessType_Default) {
          BlobChild* actor
            = ContentChild::GetSingleton()->GetOrCreateActorForBlob(mBlob);
          if (!actor) {
            return NS_ERROR_FAILURE;
          }

          DeviceStorageAddParams params;
          params.blobChild() = actor;
          params.type() = mFile->mStorageType;
          params.storageName() = mFile->mStorageName;
          params.relpath() = mFile->mPath;

          PDeviceStorageRequestChild* child
            = new DeviceStorageRequestChild(mRequest, mFile);
          ContentChild::GetSingleton()
            ->SendPDeviceStorageRequestConstructor(child, params);
          return NS_OK;
        }

        DOMFile* blob = static_cast<DOMFile*>(mBlob.get());
        r = new WriteFileEvent(blob->Impl(), mFile, mRequest.forget(),
                               mRequestType);
        break;
      }

      case DEVICE_STORAGE_REQUEST_APPEND:
      {
        if (!mBlob || !mFile->mFile) {
          return NS_ERROR_FAILURE;
        }

        DeviceStorageTypeChecker* typeChecker
          = DeviceStorageTypeChecker::CreateOrGet();
        if (!typeChecker) {
          return NS_OK;
        }

        if (!typeChecker->Check(mFile->mStorageType, mFile->mFile) ||
            !typeChecker->Check(mFile->mStorageType, mBlob)) {
          r = new PostErrorEvent(mRequest.forget(),
                                 POST_ERROR_EVENT_ILLEGAL_TYPE);
          return NS_DispatchToCurrentThread(r);
        }

        if (XRE_GetProcessType() != GeckoProcessType_Default) {
          BlobChild* actor
            = ContentChild::GetSingleton()->GetOrCreateActorForBlob(mBlob);
          if (!actor) {
            return NS_ERROR_FAILURE;
          }

          DeviceStorageAppendParams params;
          params.blobChild() = actor;
          params.type() = mFile->mStorageType;
          params.storageName() = mFile->mStorageName;
          params.relpath() = mFile->mPath;

          PDeviceStorageRequestChild* child
            = new DeviceStorageRequestChild(mRequest, mFile);
          ContentChild::GetSingleton()
            ->SendPDeviceStorageRequestConstructor(child, params);
          return NS_OK;
        }

        DOMFile* blob = static_cast<DOMFile*>(mBlob.get());
        r = new WriteFileEvent(blob->Impl(), mFile, mRequest.forget(),
                               mRequestType);
        break;
      }

      case DEVICE_STORAGE_REQUEST_READ:
      case DEVICE_STORAGE_REQUEST_WRITE:
      {
        if (!mFile->mFile) {
          return NS_ERROR_FAILURE;
        }

        DeviceStorageTypeChecker* typeChecker
          = DeviceStorageTypeChecker::CreateOrGet();
        if (!typeChecker) {
          return NS_OK;
        }

        if (!typeChecker->Check(mFile->mStorageType, mFile->mFile)) {
          r = new PostErrorEvent(mRequest.forget(),
                                 POST_ERROR_EVENT_ILLEGAL_TYPE);
          return NS_DispatchToCurrentThread(r);
        }

        if (XRE_GetProcessType() != GeckoProcessType_Default) {
          PDeviceStorageRequestChild* child
            = new DeviceStorageRequestChild(mRequest, mFile);
          DeviceStorageGetParams params(mFile->mStorageType,
                                        mFile->mStorageName,
                                        mFile->mRootDir,
                                        mFile->mPath);
          ContentChild::GetSingleton()
            ->SendPDeviceStorageRequestConstructor(child, params);
          return NS_OK;
        }

        r = new ReadFileEvent(mFile, mRequest.forget());
        break;
      }

      case DEVICE_STORAGE_REQUEST_DELETE:
      {
        if (!mFile->mFile) {
          return NS_ERROR_FAILURE;
        }

        DeviceStorageTypeChecker* typeChecker
          = DeviceStorageTypeChecker::CreateOrGet();
        if (!typeChecker) {
          return NS_OK;
        }

        if (!typeChecker->Check(mFile->mStorageType, mFile->mFile)) {
          r = new PostErrorEvent(mRequest.forget(),
                                 POST_ERROR_EVENT_ILLEGAL_TYPE);
          return NS_DispatchToCurrentThread(r);
        }

        if (XRE_GetProcessType() != GeckoProcessType_Default) {
          PDeviceStorageRequestChild* child
            = new DeviceStorageRequestChild(mRequest, mFile);
          DeviceStorageDeleteParams params(mFile->mStorageType,
                                           mFile->mStorageName,
                                           mFile->mPath);
          ContentChild::GetSingleton()
            ->SendPDeviceStorageRequestConstructor(child, params);
          return NS_OK;
        }
        r = new DeleteFileEvent(mFile, mRequest.forget());
        break;
      }

      case DEVICE_STORAGE_REQUEST_FREE_SPACE:
      {
        if (XRE_GetProcessType() != GeckoProcessType_Default) {
          PDeviceStorageRequestChild* child
            = new DeviceStorageRequestChild(mRequest, mFile);
          DeviceStorageFreeSpaceParams params(mFile->mStorageType,
                                              mFile->mStorageName);
          ContentChild::GetSingleton()
            ->SendPDeviceStorageRequestConstructor(child, params);
          return NS_OK;
        }
        r = new FreeSpaceFileEvent(mFile, mRequest.forget());
        break;
      }

      case DEVICE_STORAGE_REQUEST_USED_SPACE:
      {
        if (XRE_GetProcessType() != GeckoProcessType_Default) {
          PDeviceStorageRequestChild* child
            = new DeviceStorageRequestChild(mRequest, mFile);
          DeviceStorageUsedSpaceParams params(mFile->mStorageType,
                                              mFile->mStorageName);
          ContentChild::GetSingleton()
            ->SendPDeviceStorageRequestConstructor(child, params);
          return NS_OK;
        }
        // this needs to be dispatched to only one (1)
        // thread or we will do more work than required.
        DeviceStorageUsedSpaceCache* usedSpaceCache
          = DeviceStorageUsedSpaceCache::CreateOrGet();
        MOZ_ASSERT(usedSpaceCache);
        r = new UsedSpaceFileEvent(mFile, mRequest.forget());
        usedSpaceCache->Dispatch(r);
        return NS_OK;
      }

      case DEVICE_STORAGE_REQUEST_AVAILABLE:
      {
        if (XRE_GetProcessType() != GeckoProcessType_Default) {
          PDeviceStorageRequestChild* child
            = new DeviceStorageRequestChild(mRequest, mFile);
          DeviceStorageAvailableParams params(mFile->mStorageType,
                                              mFile->mStorageName);
          ContentChild::GetSingleton()
            ->SendPDeviceStorageRequestConstructor(child, params);
          return NS_OK;
        }
        r = new PostAvailableResultEvent(mFile, mRequest);
        return NS_DispatchToCurrentThread(r);
      }

      case DEVICE_STORAGE_REQUEST_STATUS:
      {
        if (XRE_GetProcessType() != GeckoProcessType_Default) {
          PDeviceStorageRequestChild* child
            = new DeviceStorageRequestChild(mRequest, mFile);
          DeviceStorageStatusParams params(mFile->mStorageType,
                                              mFile->mStorageName);
          ContentChild::GetSingleton()
            ->SendPDeviceStorageRequestConstructor(child, params);
          return NS_OK;
        }
        r = new PostStatusResultEvent(mFile, mRequest);
        return NS_DispatchToCurrentThread(r);
      }

      case DEVICE_STORAGE_REQUEST_WATCH:
      {
        mDeviceStorage->mAllowedToWatchFile = true;
        return NS_OK;
      }

      case DEVICE_STORAGE_REQUEST_FORMAT:
      {
        if (XRE_GetProcessType() != GeckoProcessType_Default) {
          PDeviceStorageRequestChild* child
            = new DeviceStorageRequestChild(mRequest, mFile);
          DeviceStorageFormatParams params(mFile->mStorageType,
                                           mFile->mStorageName);
          ContentChild::GetSingleton()
            ->SendPDeviceStorageRequestConstructor(child, params);
          return NS_OK;
        }
        r = new PostFormatResultEvent(mFile, mRequest);
        return NS_DispatchToCurrentThread(r);
      }

      case DEVICE_STORAGE_REQUEST_MOUNT:
      {
        if (XRE_GetProcessType() != GeckoProcessType_Default) {
          PDeviceStorageRequestChild* child
            = new DeviceStorageRequestChild(mRequest, mFile);
          DeviceStorageMountParams params(mFile->mStorageType,
                                           mFile->mStorageName);
          ContentChild::GetSingleton()
            ->SendPDeviceStorageRequestConstructor(child, params);
          return NS_OK;
        }
        r = new PostMountResultEvent(mFile, mRequest);
        return NS_DispatchToCurrentThread(r);
      }

      case DEVICE_STORAGE_REQUEST_UNMOUNT:
      {
        if (XRE_GetProcessType() != GeckoProcessType_Default) {
          PDeviceStorageRequestChild* child
            = new DeviceStorageRequestChild(mRequest, mFile);
          DeviceStorageUnmountParams params(mFile->mStorageType,
                                           mFile->mStorageName);
          ContentChild::GetSingleton()
            ->SendPDeviceStorageRequestConstructor(child, params);
          return NS_OK;
        }
        r = new PostUnmountResultEvent(mFile, mRequest);
        return NS_DispatchToCurrentThread(r);
      }
    }

    if (r) {
      nsCOMPtr<nsIEventTarget> target
        = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
      MOZ_ASSERT(target);
      target->Dispatch(r, NS_DISPATCH_NORMAL);
    }

    return NS_OK;
  }

private:
  ~DeviceStorageRequest() {}

  int32_t mRequestType;
  nsCOMPtr<nsPIDOMWindow> mWindow;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsRefPtr<DeviceStorageFile> mFile;

  nsRefPtr<DOMRequest> mRequest;
  nsCOMPtr<nsIDOMBlob> mBlob;
  nsRefPtr<nsDOMDeviceStorage> mDeviceStorage;
  nsRefPtr<DeviceStorageFileDescriptor> mDSFileDescriptor;
};

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DeviceStorageRequest)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContentPermissionRequest)
  NS_INTERFACE_MAP_ENTRY(nsIContentPermissionRequest)
  NS_INTERFACE_MAP_ENTRY(nsIRunnable)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DeviceStorageRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DeviceStorageRequest)

NS_IMPL_CYCLE_COLLECTION(DeviceStorageRequest,
                         mRequest,
                         mWindow,
                         mBlob,
                         mDeviceStorage)


NS_INTERFACE_MAP_BEGIN(nsDOMDeviceStorage)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDeviceStorage)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(nsDOMDeviceStorage, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(nsDOMDeviceStorage, DOMEventTargetHelper)

nsDOMDeviceStorage::nsDOMDeviceStorage(nsPIDOMWindow* aWindow)
  : DOMEventTargetHelper(aWindow)
  , mIsShareable(false)
  , mIsWatchingFile(false)
  , mAllowedToWatchFile(false)
{
}

/* virtual */ JSObject*
nsDOMDeviceStorage::WrapObject(JSContext* aCx)
{
  return DeviceStorageBinding::Wrap(aCx, this);
}

nsresult
nsDOMDeviceStorage::Init(nsPIDOMWindow* aWindow, const nsAString &aType,
                         const nsAString &aVolName)
{
  DebugOnly<FileUpdateDispatcher*> observer
    = FileUpdateDispatcher::GetSingleton();
  MOZ_ASSERT(observer);

  MOZ_ASSERT(aWindow);

  SetRootDirectoryForType(aType, aVolName);
  if (!mRootDirectory) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if (!mStorageName.IsEmpty()) {
    RegisterForSDCardChanges(this);

#ifdef MOZ_WIDGET_GONK
    if (DeviceStorageTypeChecker::IsVolumeBased(mStorageType)) {
      nsCOMPtr<nsIVolumeService> vs = do_GetService(NS_VOLUMESERVICE_CONTRACTID);
      if (NS_WARN_IF(!vs)) {
        return NS_ERROR_FAILURE;
      }
      nsresult rv;
      nsCOMPtr<nsIVolume> vol;
      rv = vs->GetVolumeByName(mStorageName, getter_AddRefs(vol));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      bool isFake;
      rv = vol->GetIsFake(&isFake);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      mIsShareable = !isFake;
    }
#endif
  }

  // Grab the principal of the document
  nsCOMPtr<nsIDocument> doc = aWindow->GetDoc();
  if (!doc) {
    return NS_ERROR_FAILURE;
  }
  mPrincipal = doc->NodePrincipal();

  // the 'apps' type is special.  We only want this exposed
  // if the caller has the "webapps-manage" permission.
  if (aType.EqualsLiteral(DEVICESTORAGE_APPS)) {
    nsCOMPtr<nsIPermissionManager> permissionManager
      = services::GetPermissionManager();
    NS_ENSURE_TRUE(permissionManager, NS_ERROR_FAILURE);

    uint32_t permission;
    nsresult rv
      = permissionManager->TestPermissionFromPrincipal(mPrincipal,
                                                       "webapps-manage",
                                                       &permission);

    if (NS_FAILED(rv) || permission != nsIPermissionManager::ALLOW_ACTION) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  return NS_OK;
}

nsDOMDeviceStorage::~nsDOMDeviceStorage()
{
}

void
nsDOMDeviceStorage::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mFileSystem) {
    mFileSystem->Shutdown();
    mFileSystem = nullptr;
  }

  if (!mStorageName.IsEmpty()) {
    UnregisterForSDCardChanges(this);
  }

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  obs->RemoveObserver(this, "file-watcher-update");
  obs->RemoveObserver(this, "disk-space-watcher");
}

StaticAutoPtr<nsTArray<nsString>> nsDOMDeviceStorage::sVolumeNameCache;

// static
void
nsDOMDeviceStorage::GetOrderedVolumeNames(
  nsDOMDeviceStorage::VolumeNameArray &aVolumeNames)
{
  if (sVolumeNameCache && sVolumeNameCache->Length() > 0) {
    aVolumeNames.AppendElements(*sVolumeNameCache);
    return;
  }
#ifdef MOZ_WIDGET_GONK
  nsCOMPtr<nsIVolumeService> vs = do_GetService(NS_VOLUMESERVICE_CONTRACTID);
  if (vs) {
    nsCOMPtr<nsIArray> volNames;
    vs->GetVolumeNames(getter_AddRefs(volNames));
    uint32_t length = -1;
    volNames->GetLength(&length);
    for (uint32_t i = 0; i < length; i++) {
      nsCOMPtr<nsISupportsString> str = do_QueryElementAt(volNames, i);
      if (str) {
        nsAutoString s;
        if (NS_SUCCEEDED(str->GetData(s)) && !s.IsEmpty()) {
          aVolumeNames.AppendElement(s);
        }
      }
    }

    // If the volume sdcard exists, then we want it to be first.

    VolumeNameArray::index_type sdcardIndex;
    sdcardIndex = aVolumeNames.IndexOf(NS_LITERAL_STRING("sdcard"));
    if (sdcardIndex != VolumeNameArray::NoIndex && sdcardIndex > 0) {
      aVolumeNames.RemoveElementAt(sdcardIndex);
      aVolumeNames.InsertElementAt(0, NS_LITERAL_STRING("sdcard"));
    }
  }
#endif
  if (aVolumeNames.IsEmpty()) {
    aVolumeNames.AppendElement(EmptyString());
  }
  sVolumeNameCache = new nsTArray<nsString>;
  sVolumeNameCache->AppendElements(aVolumeNames);
}

// static
void
nsDOMDeviceStorage::CreateDeviceStorageFor(nsPIDOMWindow* aWin,
                                           const nsAString &aType,
                                           nsDOMDeviceStorage** aStore)
{
  nsString storageName;
  if (!DeviceStorageTypeChecker::IsVolumeBased(aType)) {
    // The storage name will be the empty string
    storageName.Truncate();
  } else {
    GetDefaultStorageName(aType, storageName);
  }

  nsRefPtr<nsDOMDeviceStorage> ds = new nsDOMDeviceStorage(aWin);
  if (NS_FAILED(ds->Init(aWin, aType, storageName))) {
    *aStore = nullptr;
    return;
  }
  NS_ADDREF(*aStore = ds.get());
}

// static
void
nsDOMDeviceStorage::CreateDeviceStoragesFor(
  nsPIDOMWindow* aWin,
  const nsAString &aType,
  nsTArray<nsRefPtr<nsDOMDeviceStorage> > &aStores)
{
  nsresult rv;

  if (!DeviceStorageTypeChecker::IsVolumeBased(aType)) {
    nsRefPtr<nsDOMDeviceStorage> storage = new nsDOMDeviceStorage(aWin);
    rv = storage->Init(aWin, aType, EmptyString());
    if (NS_SUCCEEDED(rv)) {
      aStores.AppendElement(storage);
    }
    return;
  }
  VolumeNameArray volNames;
  GetOrderedVolumeNames(volNames);

  VolumeNameArray::size_type numVolumeNames = volNames.Length();
  for (VolumeNameArray::index_type i = 0; i < numVolumeNames; i++) {
    nsRefPtr<nsDOMDeviceStorage> storage = new nsDOMDeviceStorage(aWin);
    rv = storage->Init(aWin, aType, volNames[i]);
    if (NS_FAILED(rv)) {
      break;
    }
    aStores.AppendElement(storage);
  }
}

// static
bool
nsDOMDeviceStorage::ParseFullPath(const nsAString& aFullPath,
                                  nsAString& aOutStorageName,
                                  nsAString& aOutStoragePath)
{
  aOutStorageName.Truncate();
  aOutStoragePath.Truncate();

  NS_NAMED_LITERAL_STRING(slash, "/");

  nsDependentSubstring storageName;

  if (StringBeginsWith(aFullPath, slash)) {
    int32_t slashIndex = aFullPath.FindChar('/', 1);
    if (slashIndex == kNotFound) {
      // names of the form /filename are illegal
      return false;
    }
    storageName.Rebind(aFullPath, 1, slashIndex - 1);
    aOutStoragePath = Substring(aFullPath, slashIndex + 1);
  } else {
    aOutStoragePath = aFullPath;
  }
  // If no volume name was specified in aFullPath, then aOutStorageName
  // will wind up being the empty string. It's up to the caller to figure
  // out which storage name to actually use.
  aOutStorageName = storageName;
  return true;
}

already_AddRefed<nsDOMDeviceStorage>
nsDOMDeviceStorage::GetStorage(const nsAString& aFullPath,
                               nsAString& aOutStoragePath)
{
  nsString storageName;
  if (!ParseFullPath(aFullPath, storageName, aOutStoragePath)) {
    return nullptr;
  }
  nsRefPtr<nsDOMDeviceStorage> ds;
  if (storageName.IsEmpty()) {
    ds = this;
  } else {
    ds = GetStorageByName(storageName);
  }
  return ds.forget();
}

already_AddRefed<nsDOMDeviceStorage>
nsDOMDeviceStorage::GetStorageByName(const nsAString& aStorageName)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<nsDOMDeviceStorage> ds;

  if (mStorageName.Equals(aStorageName)) {
    ds = this;
    return ds.forget();
  }
  VolumeNameArray volNames;
  GetOrderedVolumeNames(volNames);
  VolumeNameArray::size_type numVolumes = volNames.Length();
  VolumeNameArray::index_type i;
  for (i = 0; i < numVolumes; i++) {
    if (volNames[i].Equals(aStorageName)) {
      ds = new nsDOMDeviceStorage(GetOwner());
      nsresult rv = ds->Init(GetOwner(), mStorageType, aStorageName);
      if (NS_FAILED(rv)) {
        return nullptr;
      }
      return ds.forget();
    }
  }
  return nullptr;
}

// static
void
nsDOMDeviceStorage::GetDefaultStorageName(const nsAString& aStorageType,
                                          nsAString& aStorageName)
{
  // See if the preferred volume is available.
  nsRefPtr<nsDOMDeviceStorage> ds;
  nsAdoptingString prefStorageName =
    mozilla::Preferences::GetString("device.storage.writable.name");
  if (prefStorageName) {
    aStorageName = prefStorageName;
    return;
  }

  // No preferred storage, we'll use the first one (which should be sdcard).

  VolumeNameArray volNames;
  GetOrderedVolumeNames(volNames);
  if (volNames.Length() > 0) {
    aStorageName = volNames[0];
    return;
  }

  // No volumes available, return the empty string. This is normal for
  // b2g-desktop.
  aStorageName.Truncate();
}

bool
nsDOMDeviceStorage::IsAvailable()
{
  nsRefPtr<DeviceStorageFile> dsf(new DeviceStorageFile(mStorageType, mStorageName));
  return dsf->IsAvailable();
}

NS_IMETHODIMP
nsDOMDeviceStorage::Add(nsIDOMBlob *aBlob, nsIDOMDOMRequest * *_retval)
{
  ErrorResult rv;
  nsRefPtr<DOMRequest> request = Add(aBlob, rv);
  request.forget(_retval);
  return rv.ErrorCode();
}

already_AddRefed<DOMRequest>
nsDOMDeviceStorage::Add(nsIDOMBlob* aBlob, ErrorResult& aRv)
{
  if (!aBlob) {
    return nullptr;
  }

  nsCOMPtr<nsIMIMEService> mimeSvc = do_GetService(NS_MIMESERVICE_CONTRACTID);
  if (!mimeSvc) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // if mimeType isn't set, we will not get a correct
  // extension, and AddNamed() will fail.  This will post an
  // onerror to the requestee.
  nsString mimeType;
  aBlob->GetType(mimeType);

  nsCString extension;
  mimeSvc->GetPrimaryExtension(NS_LossyConvertUTF16toASCII(mimeType),
                               EmptyCString(), extension);
  // if extension is null here, we will ignore it for now.
  // AddNamed() will check the file path and fail.  This
  // will post an onerror to the requestee.

  // possible race here w/ unique filename
  char buffer[32];
  NS_MakeRandomString(buffer, ArrayLength(buffer) - 1);

  nsAutoCString path;
  path.Assign(nsDependentCString(buffer));
  path.Append('.');
  path.Append(extension);

  return AddNamed(aBlob, NS_ConvertASCIItoUTF16(path), aRv);
}

NS_IMETHODIMP
nsDOMDeviceStorage::AddNamed(nsIDOMBlob *aBlob,
                             const nsAString & aPath,
                             nsIDOMDOMRequest * *_retval)
{
  ErrorResult rv;
  nsRefPtr<DOMRequest> request = AddNamed(aBlob, aPath, rv);
  request.forget(_retval);
  return rv.ErrorCode();
}

already_AddRefed<DOMRequest>
nsDOMDeviceStorage::AddNamed(nsIDOMBlob* aBlob, const nsAString& aPath,
                             ErrorResult& aRv)
{
  return AddOrAppendNamed(aBlob, aPath,
                          DEVICE_STORAGE_REQUEST_CREATE, aRv);
}

already_AddRefed<DOMRequest>
nsDOMDeviceStorage::AppendNamed(nsIDOMBlob* aBlob, const nsAString& aPath,
                                ErrorResult& aRv)
{
  return AddOrAppendNamed(aBlob, aPath,
                          DEVICE_STORAGE_REQUEST_APPEND, aRv);
}


already_AddRefed<DOMRequest>
nsDOMDeviceStorage::AddOrAppendNamed(nsIDOMBlob* aBlob, const nsAString& aPath,
                                     const int32_t aRequestType, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

  // if the blob is null here, bail
  if (!aBlob) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  DeviceStorageTypeChecker* typeChecker
    = DeviceStorageTypeChecker::CreateOrGet();
  if (!typeChecker) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIRunnable> r;
  nsresult rv;

  if (IsFullPath(aPath)) {
    nsString storagePath;
    nsRefPtr<nsDOMDeviceStorage> ds = GetStorage(aPath, storagePath);
    if (!ds) {
      nsRefPtr<DOMRequest> request = new DOMRequest(win);
      r = new PostErrorEvent(request, POST_ERROR_EVENT_UNKNOWN);
      rv = NS_DispatchToCurrentThread(r);
      if (NS_FAILED(rv)) {
        aRv.Throw(rv);
      }
      return request.forget();
    }

    return ds->AddOrAppendNamed(aBlob, storagePath,
                                aRequestType, aRv);
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType,
                                                          mStorageName,
                                                          aPath);
  if (!dsf->IsSafePath()) {
    r = new PostErrorEvent(request, POST_ERROR_EVENT_PERMISSION_DENIED);
  } else if (!typeChecker->Check(mStorageType, dsf->mFile) ||
      !typeChecker->Check(mStorageType, aBlob)) {
    r = new PostErrorEvent(request, POST_ERROR_EVENT_ILLEGAL_TYPE);
  } else if (aRequestType == DEVICE_STORAGE_REQUEST_APPEND ||
             aRequestType == DEVICE_STORAGE_REQUEST_CREATE) {
    r = new DeviceStorageRequest(DeviceStorageRequestType(aRequestType),
                                 win, mPrincipal, dsf, request, aBlob);
  } else {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
  }

  rv = NS_DispatchToCurrentThread(r);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
  return request.forget();
}

NS_IMETHODIMP
nsDOMDeviceStorage::Get(const nsAString& aPath, nsIDOMDOMRequest** aRetval)
{
  ErrorResult rv;
  nsRefPtr<DOMRequest> request = Get(aPath, rv);
  request.forget(aRetval);
  return rv.ErrorCode();
}

NS_IMETHODIMP
nsDOMDeviceStorage::GetEditable(const nsAString& aPath,
                                nsIDOMDOMRequest** aRetval)
{
  ErrorResult rv;
  nsRefPtr<DOMRequest> request = GetEditable(aPath, rv);
  request.forget(aRetval);
  return rv.ErrorCode();
}

already_AddRefed<DOMRequest>
nsDOMDeviceStorage::GetInternal(const nsAString& aPath, bool aEditable,
                                ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);

  if (IsFullPath(aPath)) {
    nsString storagePath;
    nsRefPtr<nsDOMDeviceStorage> ds = GetStorage(aPath, storagePath);
    if (!ds) {
      nsCOMPtr<nsIRunnable> r =
        new PostErrorEvent(request, POST_ERROR_EVENT_UNKNOWN);
      nsresult rv = NS_DispatchToCurrentThread(r);
      if (NS_FAILED(rv)) {
        aRv.Throw(rv);
      }
      return request.forget();
    }
    ds->GetInternal(win, storagePath, request, aEditable);
    return request.forget();
  }
  GetInternal(win, aPath, request, aEditable);
  return request.forget();
}

void
nsDOMDeviceStorage::GetInternal(nsPIDOMWindow *aWin,
                                const nsAString& aPath,
                                DOMRequest* aRequest,
                                bool aEditable)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType,
                                                          mStorageName,
                                                          aPath);
  dsf->SetEditable(aEditable);

  nsCOMPtr<nsIRunnable> r;
  if (!dsf->IsSafePath()) {
    r = new PostErrorEvent(aRequest, POST_ERROR_EVENT_PERMISSION_DENIED);
  } else {
    r = new DeviceStorageRequest(aEditable ? DEVICE_STORAGE_REQUEST_WRITE
                                           : DEVICE_STORAGE_REQUEST_READ,
                                 aWin, mPrincipal, dsf, aRequest);
  }
  DebugOnly<nsresult> rv = NS_DispatchToCurrentThread(r);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

NS_IMETHODIMP
nsDOMDeviceStorage::Delete(const nsAString& aPath, nsIDOMDOMRequest** aRetval)
{
  ErrorResult rv;
  nsRefPtr<DOMRequest> request = Delete(aPath, rv);
  request.forget(aRetval);
  return rv.ErrorCode();
}

already_AddRefed<DOMRequest>
nsDOMDeviceStorage::Delete(const nsAString& aPath, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);

  if (IsFullPath(aPath)) {
    nsString storagePath;
    nsRefPtr<nsDOMDeviceStorage> ds = GetStorage(aPath, storagePath);
    if (!ds) {
      nsCOMPtr<nsIRunnable> r =
        new PostErrorEvent(request, POST_ERROR_EVENT_UNKNOWN);
      nsresult rv = NS_DispatchToCurrentThread(r);
      if (NS_FAILED(rv)) {
        aRv.Throw(rv);
      }
      return request.forget();
    }
    ds->DeleteInternal(win, storagePath, request);
    return request.forget();
  }
  DeleteInternal(win, aPath, request);
  return request.forget();
}

void
nsDOMDeviceStorage::DeleteInternal(nsPIDOMWindow *aWin,
                                   const nsAString& aPath,
                                   DOMRequest* aRequest)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIRunnable> r;
  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType,
                                                          mStorageName,
                                                          aPath);
  if (!dsf->IsSafePath()) {
    r = new PostErrorEvent(aRequest, POST_ERROR_EVENT_PERMISSION_DENIED);
  } else {
    r = new DeviceStorageRequest(DEVICE_STORAGE_REQUEST_DELETE,
                                 aWin, mPrincipal, dsf, aRequest);
  }
  DebugOnly<nsresult> rv = NS_DispatchToCurrentThread(r);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

NS_IMETHODIMP
nsDOMDeviceStorage::FreeSpace(nsIDOMDOMRequest** aRetval)
{
  ErrorResult rv;
  nsRefPtr<DOMRequest> request = FreeSpace(rv);
  request.forget(aRetval);
  return rv.ErrorCode();
}

already_AddRefed<DOMRequest>
nsDOMDeviceStorage::FreeSpace(ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType,
                                                          mStorageName);
  nsCOMPtr<nsIRunnable> r
    = new DeviceStorageRequest(DEVICE_STORAGE_REQUEST_FREE_SPACE,
                               win, mPrincipal, dsf, request);
  nsresult rv = NS_DispatchToCurrentThread(r);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
  return request.forget();
}

NS_IMETHODIMP
nsDOMDeviceStorage::UsedSpace(nsIDOMDOMRequest** aRetval)
{
  ErrorResult rv;
  nsRefPtr<DOMRequest> request = UsedSpace(rv);
  request.forget(aRetval);
  return rv.ErrorCode();
}

already_AddRefed<DOMRequest>
nsDOMDeviceStorage::UsedSpace(ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  DebugOnly<DeviceStorageUsedSpaceCache*> usedSpaceCache
    = DeviceStorageUsedSpaceCache::CreateOrGet();
  MOZ_ASSERT(usedSpaceCache);

  nsRefPtr<DOMRequest> request = new DOMRequest(win);

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType,
                                                          mStorageName);
  nsCOMPtr<nsIRunnable> r
    = new DeviceStorageRequest(DEVICE_STORAGE_REQUEST_USED_SPACE,
                               win, mPrincipal, dsf, request);
  nsresult rv = NS_DispatchToCurrentThread(r);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
  return request.forget();
}

NS_IMETHODIMP
nsDOMDeviceStorage::Available(nsIDOMDOMRequest** aRetval)
{
  ErrorResult rv;
  nsRefPtr<DOMRequest> request = Available(rv);
  request.forget(aRetval);
  return rv.ErrorCode();
}

already_AddRefed<DOMRequest>
nsDOMDeviceStorage::Available(ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType,
                                                          mStorageName);
  nsCOMPtr<nsIRunnable> r
    = new DeviceStorageRequest(DEVICE_STORAGE_REQUEST_AVAILABLE,
                               win, mPrincipal, dsf, request);
  nsresult rv = NS_DispatchToCurrentThread(r);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
  return request.forget();
}

already_AddRefed<DOMRequest>
nsDOMDeviceStorage::StorageStatus(ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType,
                                                          mStorageName);
  nsCOMPtr<nsIRunnable> r
    = new DeviceStorageRequest(DEVICE_STORAGE_REQUEST_STATUS,
                               win, mPrincipal, dsf, request);
  nsresult rv = NS_DispatchToCurrentThread(r);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
  return request.forget();
}

already_AddRefed<DOMRequest>
nsDOMDeviceStorage::Format(ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType,
                                                          mStorageName);
  nsCOMPtr<nsIRunnable> r
    = new DeviceStorageRequest(DEVICE_STORAGE_REQUEST_FORMAT,
                               win, mPrincipal, dsf, request);
  nsresult rv = NS_DispatchToCurrentThread(r);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
  return request.forget();
}

already_AddRefed<DOMRequest>
nsDOMDeviceStorage::Mount(ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType,
                                                          mStorageName);
  nsCOMPtr<nsIRunnable> r
    = new DeviceStorageRequest(DEVICE_STORAGE_REQUEST_MOUNT,
                               win, mPrincipal, dsf, request);
  nsresult rv = NS_DispatchToCurrentThread(r);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
  return request.forget();
}

already_AddRefed<DOMRequest>
nsDOMDeviceStorage::Unmount(ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType,
                                                          mStorageName);
  nsCOMPtr<nsIRunnable> r
    = new DeviceStorageRequest(DEVICE_STORAGE_REQUEST_UNMOUNT,
                               win, mPrincipal, dsf, request);
  nsresult rv = NS_DispatchToCurrentThread(r);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
  return request.forget();
}

NS_IMETHODIMP
nsDOMDeviceStorage::CreateFileDescriptor(const nsAString& aPath,
                                         DeviceStorageFileDescriptor* aDSFileDescriptor,
                                         nsIDOMDOMRequest** aRequest)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aDSFileDescriptor);

  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    return NS_ERROR_UNEXPECTED;
  }

  DeviceStorageTypeChecker* typeChecker
    = DeviceStorageTypeChecker::CreateOrGet();
  if (!typeChecker) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIRunnable> r;
  nsresult rv;

  if (IsFullPath(aPath)) {
    nsString storagePath;
    nsRefPtr<nsDOMDeviceStorage> ds = GetStorage(aPath, storagePath);
    if (!ds) {
      nsRefPtr<DOMRequest> request = new DOMRequest(win);
      r = new PostErrorEvent(request, POST_ERROR_EVENT_UNKNOWN);
      rv = NS_DispatchToCurrentThread(r);
      if (NS_FAILED(rv)) {
        return rv;
      }
      request.forget(aRequest);
      return NS_OK;
    }
    return ds->CreateFileDescriptor(storagePath, aDSFileDescriptor, aRequest);
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType,
                                                          mStorageName,
                                                          aPath);
  if (!dsf->IsSafePath()) {
    r = new PostErrorEvent(request, POST_ERROR_EVENT_PERMISSION_DENIED);
  } else if (!typeChecker->Check(mStorageType, dsf->mFile)) {
    r = new PostErrorEvent(request, POST_ERROR_EVENT_ILLEGAL_TYPE);
  } else {
    r = new DeviceStorageRequest(DEVICE_STORAGE_REQUEST_CREATEFD,
                                 win, mPrincipal, dsf, request,
                                 aDSFileDescriptor);
  }

  rv = NS_DispatchToCurrentThread(r);
  if (NS_FAILED(rv)) {
    return rv;
  }
  request.forget(aRequest);
  return NS_OK;
}

bool
nsDOMDeviceStorage::Default()
{
  nsString defaultStorageName;
  GetDefaultStorageName(mStorageType, defaultStorageName);
  return mStorageName.Equals(defaultStorageName);
}

bool
nsDOMDeviceStorage::CanBeFormatted()
{
  // Currently, any volume which can be shared can also be formatted.
  return mIsShareable;
}

bool
nsDOMDeviceStorage::CanBeMounted()
{
  // Currently, any volume which can be shared can also be mounted/unmounted.
  return mIsShareable;
}

bool
nsDOMDeviceStorage::CanBeShared()
{
  return mIsShareable;
}

already_AddRefed<Promise>
nsDOMDeviceStorage::GetRoot(ErrorResult& aRv)
{
  if (!mFileSystem) {
    mFileSystem = new DeviceStorageFileSystem(mStorageType, mStorageName);
    mFileSystem->Init(this);
  }
  return mozilla::dom::Directory::GetRoot(mFileSystem, aRv);
}

NS_IMETHODIMP
nsDOMDeviceStorage::GetDefault(bool* aDefault)
{
  *aDefault = Default();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceStorage::GetStorageName(nsAString& aStorageName)
{
  aStorageName = mStorageName;
  return NS_OK;
}

already_AddRefed<DOMCursor>
nsDOMDeviceStorage::Enumerate(const nsAString& aPath,
                              const EnumerationParameters& aOptions,
                              ErrorResult& aRv)
{
  return EnumerateInternal(aPath, aOptions, false, aRv);
}

already_AddRefed<DOMCursor>
nsDOMDeviceStorage::EnumerateEditable(const nsAString& aPath,
                                      const EnumerationParameters& aOptions,
                                      ErrorResult& aRv)
{
  return EnumerateInternal(aPath, aOptions, true, aRv);
}


already_AddRefed<DOMCursor>
nsDOMDeviceStorage::EnumerateInternal(const nsAString& aPath,
                                      const EnumerationParameters& aOptions,
                                      bool aEditable, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  PRTime since = 0;
  if (aOptions.mSince.WasPassed() && !aOptions.mSince.Value().IsUndefined()) {
    since = PRTime(aOptions.mSince.Value().TimeStamp());
  }

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType,
                                                          mStorageName,
                                                          aPath,
                                                          EmptyString());
  dsf->SetEditable(aEditable);

  nsRefPtr<nsDOMDeviceStorageCursor> cursor
    = new nsDOMDeviceStorageCursor(win, mPrincipal, dsf, since);
  nsRefPtr<DeviceStorageCursorRequest> r
    = new DeviceStorageCursorRequest(cursor);

  if (mozilla::Preferences::GetBool("device.storage.prompt.testing", false)) {
    r->Allow(JS::UndefinedHandleValue);
    return cursor.forget();
  }

  nsContentPermissionUtils::AskPermission(r, win);

  return cursor.forget();
}

#ifdef MOZ_WIDGET_GONK
void
nsDOMDeviceStorage::DispatchStatusChangeEvent(nsAString& aStatus)
{
  if (aStatus == mLastStatus) {
    // We've already sent this status, don't bother sending it again.
    return;
  }
  mLastStatus = aStatus;

  DeviceStorageChangeEventInit init;
  init.mBubbles = true;
  init.mCancelable = false;
  init.mPath = mStorageName;
  init.mReason = aStatus;

  nsRefPtr<DeviceStorageChangeEvent> event =
    DeviceStorageChangeEvent::Constructor(this, NS_LITERAL_STRING("change"),
                                          init);
  event->SetTrusted(true);

  bool ignore;
  DispatchEvent(event, &ignore);
}

void
nsDOMDeviceStorage::DispatchStorageStatusChangeEvent(nsAString& aStorageStatus)
{
  if (aStorageStatus == mLastStorageStatus) {
     // We've already sent this status, don't bother sending it again.
    return;
  }
  mLastStorageStatus = aStorageStatus;

  DeviceStorageChangeEventInit init;
  init.mBubbles = true;
  init.mCancelable = false;
  init.mPath = mStorageName;
  init.mReason = aStorageStatus;

  nsRefPtr<DeviceStorageChangeEvent> event =
    DeviceStorageChangeEvent::Constructor(this, NS_LITERAL_STRING("storage-state-change"),
                                          init);
  event->SetTrusted(true);

  bool ignore;
  DispatchEvent(event, &ignore);
}
#endif

NS_IMETHODIMP
nsDOMDeviceStorage::Observe(nsISupports *aSubject,
                            const char *aTopic,
                            const char16_t *aData)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!strcmp(aTopic, "file-watcher-update")) {

    DeviceStorageFile* file = static_cast<DeviceStorageFile*>(aSubject);
    Notify(NS_ConvertUTF16toUTF8(aData).get(), file);
    return NS_OK;
  }
  if (!strcmp(aTopic, "disk-space-watcher")) {
    // 'disk-space-watcher' notifications are sent when there is a modification
    // of a file in a specific location while a low device storage situation
    // exists or after recovery of a low storage situation. For Firefox OS,
    // these notifications are specific for apps storage.
    nsRefPtr<DeviceStorageFile> file =
      new DeviceStorageFile(mStorageType, mStorageName);
    if (!NS_strcmp(aData, MOZ_UTF16("full"))) {
      Notify("low-disk-space", file);
    } else if (!NS_strcmp(aData, MOZ_UTF16("free"))) {
      Notify("available-disk-space", file);
    }
    return NS_OK;
  }

#ifdef MOZ_WIDGET_GONK
  else if (!strcmp(aTopic, NS_VOLUME_STATE_CHANGED)) {
    // We invalidate the used space cache for the volume that actually changed
    // state.
    nsCOMPtr<nsIVolume> vol = do_QueryInterface(aSubject);
    if (!vol) {
      return NS_OK;
    }
    nsString volName;
    vol->GetName(volName);

    DeviceStorageUsedSpaceCache* usedSpaceCache
      = DeviceStorageUsedSpaceCache::CreateOrGet();
    MOZ_ASSERT(usedSpaceCache);
    usedSpaceCache->Invalidate(volName);

    if (!volName.Equals(mStorageName)) {
      // Not our volume - we can ignore.
      return NS_OK;
    }

    nsRefPtr<DeviceStorageFile> dsf(new DeviceStorageFile(mStorageType, mStorageName));
    nsString status, storageStatus;

    // Get Status (one of "available, unavailable, shared")
    dsf->GetStatus(status);
    DispatchStatusChangeEvent(status);

    // Get real volume status (defined in dom/system/gonk/nsIVolume.idl)
    dsf->GetStorageStatus(storageStatus);
    DispatchStorageStatusChangeEvent(storageStatus);
    return NS_OK;
  }
#endif
  return NS_OK;
}

nsresult
nsDOMDeviceStorage::Notify(const char* aReason, DeviceStorageFile* aFile)
{
  if (!mAllowedToWatchFile) {
    return NS_OK;
  }

  if (!mStorageType.Equals(aFile->mStorageType) ||
      !mStorageName.Equals(aFile->mStorageName)) {
    // Ignore this
    return NS_OK;
  }

  DeviceStorageChangeEventInit init;
  init.mBubbles = true;
  init.mCancelable = false;
  aFile->GetFullPath(init.mPath);
  init.mReason.AssignWithConversion(aReason);

  nsRefPtr<DeviceStorageChangeEvent> event =
    DeviceStorageChangeEvent::Constructor(this, NS_LITERAL_STRING("change"),
                                          init);
  event->SetTrusted(true);

  bool ignore;
  DispatchEvent(event, &ignore);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceStorage::AddEventListener(const nsAString & aType,
                                     nsIDOMEventListener *aListener,
                                     bool aUseCapture,
                                     bool aWantsUntrusted,
                                     uint8_t aArgc)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    return NS_ERROR_UNEXPECTED;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType,
                                                          mStorageName);
  nsCOMPtr<nsIRunnable> r
    = new DeviceStorageRequest(DEVICE_STORAGE_REQUEST_WATCH,
                               win, mPrincipal, dsf, request, this);
  nsresult rv = NS_DispatchToCurrentThread(r);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return DOMEventTargetHelper::AddEventListener(aType, aListener, aUseCapture,
                                                aWantsUntrusted, aArgc);
}

void
nsDOMDeviceStorage::AddEventListener(const nsAString & aType,
                                     EventListener *aListener,
                                     bool aUseCapture,
                                     const Nullable<bool>& aWantsUntrusted,
                                     ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType,
                                                          mStorageName);
  nsCOMPtr<nsIRunnable> r
    = new DeviceStorageRequest(DEVICE_STORAGE_REQUEST_WATCH,
                               win, mPrincipal, dsf, request, this);
  nsresult rv = NS_DispatchToCurrentThread(r);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
  DOMEventTargetHelper::AddEventListener(aType, aListener, aUseCapture,
                                         aWantsUntrusted, aRv);
}

NS_IMETHODIMP
nsDOMDeviceStorage::AddSystemEventListener(const nsAString & aType,
                                           nsIDOMEventListener *aListener,
                                           bool aUseCapture,
                                           bool aWantsUntrusted,
                                           uint8_t aArgc)
{
  if (!mIsWatchingFile) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    obs->AddObserver(this, "file-watcher-update", false);
    mIsWatchingFile = true;
  }

  return nsDOMDeviceStorage::AddEventListener(aType, aListener, aUseCapture,
                                              aWantsUntrusted, aArgc);
}

NS_IMETHODIMP
nsDOMDeviceStorage::RemoveEventListener(const nsAString & aType,
                                        nsIDOMEventListener *aListener,
                                        bool aUseCapture)
{
  DOMEventTargetHelper::RemoveEventListener(aType, aListener, false);

  if (mIsWatchingFile && !HasListenersFor(nsGkAtoms::onchange)) {
    mIsWatchingFile = false;
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    obs->RemoveObserver(this, "file-watcher-update");
  }
  return NS_OK;
}

void
nsDOMDeviceStorage::RemoveEventListener(const nsAString& aType,
                                        EventListener* aListener,
                                        bool aCapture,
                                        ErrorResult& aRv)
{
  DOMEventTargetHelper::RemoveEventListener(aType, aListener, aCapture, aRv);

  if (mIsWatchingFile && !HasListenersFor(nsGkAtoms::onchange)) {
    mIsWatchingFile = false;
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    obs->RemoveObserver(this, "file-watcher-update");
  }
}

NS_IMETHODIMP
nsDOMDeviceStorage::RemoveSystemEventListener(const nsAString & aType,
                                              nsIDOMEventListener *aListener,
                                              bool aUseCapture)
{
  return nsDOMDeviceStorage::RemoveEventListener(aType, aListener, aUseCapture);
}

NS_IMETHODIMP
nsDOMDeviceStorage::DispatchEvent(nsIDOMEvent *aEvt,
                                  bool *aRetval)
{
  return DOMEventTargetHelper::DispatchEvent(aEvt, aRetval);
}

EventTarget*
nsDOMDeviceStorage::GetTargetForDOMEvent()
{
  return DOMEventTargetHelper::GetTargetForDOMEvent();
}

EventTarget *
nsDOMDeviceStorage::GetTargetForEventTargetChain()
{
  return DOMEventTargetHelper::GetTargetForEventTargetChain();
}

nsresult
nsDOMDeviceStorage::PreHandleEvent(EventChainPreVisitor& aVisitor)
{
  return DOMEventTargetHelper::PreHandleEvent(aVisitor);
}

nsresult
nsDOMDeviceStorage::WillHandleEvent(EventChainPostVisitor& aVisitor)
{
  return DOMEventTargetHelper::WillHandleEvent(aVisitor);
}

nsresult
nsDOMDeviceStorage::PostHandleEvent(EventChainPostVisitor& aVisitor)
{
  return DOMEventTargetHelper::PostHandleEvent(aVisitor);
}

nsresult
nsDOMDeviceStorage::DispatchDOMEvent(WidgetEvent* aEvent,
                                     nsIDOMEvent* aDOMEvent,
                                     nsPresContext* aPresContext,
                                     nsEventStatus* aEventStatus)
{
  return DOMEventTargetHelper::DispatchDOMEvent(aEvent,
                                                aDOMEvent,
                                                aPresContext,
                                                aEventStatus);
}

EventListenerManager*
nsDOMDeviceStorage::GetOrCreateListenerManager()
{
  return DOMEventTargetHelper::GetOrCreateListenerManager();
}

EventListenerManager*
nsDOMDeviceStorage::GetExistingListenerManager() const
{
  return DOMEventTargetHelper::GetExistingListenerManager();
}

nsIScriptContext *
nsDOMDeviceStorage::GetContextForEventHandlers(nsresult *aRv)
{
  return DOMEventTargetHelper::GetContextForEventHandlers(aRv);
}

JSContext *
nsDOMDeviceStorage::GetJSContextForEventHandlers()
{
  return DOMEventTargetHelper::GetJSContextForEventHandlers();
}

NS_IMPL_EVENT_HANDLER(nsDOMDeviceStorage, change)
