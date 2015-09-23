/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "mozilla/dom/ipc/BlobChild.h"
#include "mozilla/dom/PBrowserChild.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/LazyIdleThread.h"
#include "mozilla/Scoped.h"
#include "mozilla/Services.h"
#include "mozilla/ipc/BackgroundUtils.h" // for PrincipalInfoToPrincipal

#include "nsArrayUtils.h"
#include "nsAutoPtr.h"
#include "nsGlobalWindow.h"
#include "nsServiceManagerUtils.h"
#include "nsIFile.h"
#include "nsIDirectoryEnumerator.h"
#include "nsNetUtil.h"
#include "nsIOutputStream.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIPrincipal.h"
#include "nsJSUtils.h"
#include "nsContentUtils.h"
#include "nsXULAppAPI.h"
#include "DeviceStorageFileDescriptor.h"
#include "DeviceStorageRequestChild.h"
#include "DeviceStorageStatics.h"
#include "nsCRT.h"
#include "nsIObserverService.h"
#include "nsIMIMEService.h"
#include "nsCExternalHandlerService.h"
#include "nsIPermissionManager.h"
#include "nsIStringBundle.h"
#include "nsISupportsPrimitives.h"
#include "nsIDocument.h"
#include <algorithm>
#include "private/pprio.h"
#include "nsContentPermissionHelper.h"

#include "mozilla/dom/DeviceStorageBinding.h"

// Microsoft's API Name hackery sucks
#undef CreateEvent

#ifdef MOZ_WIDGET_GONK
#include "nsIVolume.h"
#include "nsIVolumeService.h"
#endif

#define DEVICESTORAGE_PROPERTIES \
  "chrome://global/content/devicestorage.properties"
#define DEFAULT_THREAD_TIMEOUT_MS 30000
#define STORAGE_CHANGE_EVENT "change"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::devicestorage;
using namespace mozilla::ipc;

namespace mozilla {
  MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedPRFileDesc, PRFileDesc, PR_Close);
} // namespace mozilla

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
  dsf->GetStorageFreeSpace(&freeBytes);
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
DeviceStorageTypeChecker::Check(const nsAString& aType, BlobImpl* aBlob)
{
  MOZ_ASSERT(aBlob);

  nsString mimeType;
  aBlob->GetType(mimeType);

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
  if (!aFile) {
    return false;
  }

  nsString path;
  aFile->GetPath(path);

  return Check(aType, path);
}

bool
DeviceStorageTypeChecker::Check(const nsAString& aType, const nsString& aPath)
{
  if (aType.EqualsLiteral(DEVICESTORAGE_APPS) ||
      aType.EqualsLiteral(DEVICESTORAGE_SDCARD) ||
      aType.EqualsLiteral(DEVICESTORAGE_CRASHES)) {
    // Apps, crashes and sdcard have no restrictions on what file extensions used.
    return true;
  }

  int32_t dotIdx = aPath.RFindChar(char16_t('.'));
  if (dotIdx == kNotFound) {
    return false;
  }

  nsAutoString extensionMatch;
  extensionMatch.Assign('*');
  extensionMatch.Append(Substring(aPath, dotIdx));
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

size_t
DeviceStorageTypeChecker::GetAccessIndexForRequest(
  const DeviceStorageRequestType aRequestType)
{
  switch(aRequestType) {
    case DEVICE_STORAGE_REQUEST_READ:
    case DEVICE_STORAGE_REQUEST_WATCH:
    case DEVICE_STORAGE_REQUEST_FREE_SPACE:
    case DEVICE_STORAGE_REQUEST_USED_SPACE:
    case DEVICE_STORAGE_REQUEST_AVAILABLE:
    case DEVICE_STORAGE_REQUEST_STATUS:
    case DEVICE_STORAGE_REQUEST_CURSOR:
      return DEVICE_STORAGE_ACCESS_READ;
    case DEVICE_STORAGE_REQUEST_WRITE:
    case DEVICE_STORAGE_REQUEST_APPEND:
    case DEVICE_STORAGE_REQUEST_DELETE:
    case DEVICE_STORAGE_REQUEST_FORMAT:
    case DEVICE_STORAGE_REQUEST_MOUNT:
    case DEVICE_STORAGE_REQUEST_UNMOUNT:
      return DEVICE_STORAGE_ACCESS_WRITE;
    case DEVICE_STORAGE_REQUEST_CREATE:
    case DEVICE_STORAGE_REQUEST_CREATEFD:
      return DEVICE_STORAGE_ACCESS_CREATE;
    default:
      return DEVICE_STORAGE_ACCESS_UNDEFINED;
  }
}

nsresult
DeviceStorageTypeChecker::GetAccessForRequest(
  const DeviceStorageRequestType aRequestType, nsACString& aAccessResult)
{
  size_t access = GetAccessIndexForRequest(aRequestType);
  return GetAccessForIndex(access, aAccessResult);
}

nsresult
DeviceStorageTypeChecker::GetAccessForIndex(
  size_t aAccessIndex, nsACString& aAccessResult)
{
  static const char *names[] = { "read", "write", "create", "undefined" };
  MOZ_ASSERT(aAccessIndex < MOZ_ARRAY_LENGTH(names));
  aAccessResult.AssignASCII(names[aAccessIndex]);
  return NS_OK;
}

static bool IsMediaType(const nsAString& aType)
{
  return aType.EqualsLiteral(DEVICESTORAGE_PICTURES) ||
         aType.EqualsLiteral(DEVICESTORAGE_VIDEOS) ||
         aType.EqualsLiteral(DEVICESTORAGE_MUSIC) ||
         aType.EqualsLiteral(DEVICESTORAGE_SDCARD);
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
  return IsMediaType(aType);
#else
  return false;
#endif
}

//static
bool
DeviceStorageTypeChecker::IsSharedMediaRoot(const nsAString& aType)
{
  // This function determines if aType shares a root directory with the
  // other media types (so only applies to music, videos, pictures and sdcard).
#ifdef MOZ_WIDGET_GONK
  return IsMediaType(aType);
#else
  // For desktop, if the directories have been overridden, then they share
  // a common root.
  return IsMediaType(aType) && DeviceStorageStatics::HasOverrideRootDir();
#endif
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
  if (XRE_IsParentProcess()) {
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

  DS_LOG_INFO("type '%s' name '%s' root '%s' path '%s'",
              NS_LossyConvertUTF16toASCII(mStorageType).get(),
              NS_LossyConvertUTF16toASCII(mStorageName).get(),
              NS_LossyConvertUTF16toASCII(mRootDir).get(),
              NS_LossyConvertUTF16toASCII(mPath).get());
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
// in DeviceStorageStatics::Initialize. Directories which depend on the
// root directory of a volume should be calculated in this method.
void
DeviceStorageFile::GetRootDirectoryForType(const nsAString& aStorageType,
                                           const nsAString& aStorageName,
                                           nsIFile** aFile)
{
  nsCOMPtr<nsIFile> f;
  *aFile = nullptr;

  DeviceStorageStatics::InitializeDirs();

#ifdef MOZ_WIDGET_GONK
  nsresult rv;
  nsString volMountPoint;
  if (DeviceStorageTypeChecker::IsVolumeBased(aStorageType)) {
    nsCOMPtr<nsIVolumeService> vs = do_GetService(NS_VOLUMESERVICE_CONTRACTID);
    NS_ENSURE_TRUE_VOID(vs);
    nsCOMPtr<nsIVolume> vol;
    rv = vs->GetVolumeByName(aStorageName, getter_AddRefs(vol));
    if (NS_FAILED(rv)) {
      printf_stderr("##### DeviceStorage: GetVolumeByName('%s') failed\n",
                    NS_LossyConvertUTF16toASCII(aStorageName).get());
    }
    NS_ENSURE_SUCCESS_VOID(rv);
    vol->GetMountPoint(volMountPoint);
  }
#endif

  if (aStorageType.EqualsLiteral(DEVICESTORAGE_PICTURES)) {
    f = DeviceStorageStatics::GetPicturesDir();
  } else if (aStorageType.EqualsLiteral(DEVICESTORAGE_VIDEOS)) {
    f = DeviceStorageStatics::GetVideosDir();
  } else if (aStorageType.EqualsLiteral(DEVICESTORAGE_MUSIC)) {
    f = DeviceStorageStatics::GetMusicDir();
  } else if (aStorageType.EqualsLiteral(DEVICESTORAGE_APPS)) {
    f = DeviceStorageStatics::GetAppsDir();
  } else if (aStorageType.EqualsLiteral(DEVICESTORAGE_CRASHES)) {
    f = DeviceStorageStatics::GetCrashesDir();
  } else if (aStorageType.EqualsLiteral(DEVICESTORAGE_SDCARD)) {
    f = DeviceStorageStatics::GetSdcardDir();
  } else {
    printf_stderr("##### DeviceStorage: Unrecognized StorageType: '%s'\n",
                  NS_LossyConvertUTF16toASCII(aStorageType).get());
    return;
  }

#ifdef MOZ_WIDGET_GONK
  /* For volume based storage types, we will only have a file already
     if the override root directory option is in effect. */
  if (!f && !volMountPoint.IsEmpty()) {
    rv = NS_NewLocalFile(volMountPoint, false, getter_AddRefs(f));
    if (NS_FAILED(rv)) {
      printf_stderr("##### DeviceStorage: NS_NewLocalFile failed StorageType: '%s' path '%s'\n",
                    NS_LossyConvertUTF16toASCII(volMountPoint).get(),
                    NS_LossyConvertUTF16toASCII(aStorageType).get());
    }
  }
#endif

  if (f) {
    f->Clone(aFile);
  } else {
    // This should never happen unless something is severely wrong. So
    // scream a little.
    printf_stderr("##### GetRootDirectoryForType('%s', '%s') failed #####",
                  NS_LossyConvertUTF16toASCII(aStorageType).get(),
                  NS_LossyConvertUTF16toASCII(aStorageName).get());
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
  return CreateUnique(storageType, storageName, storagePath,
                      aFileType, aFileAttributes);
}

//static
already_AddRefed<DeviceStorageFile>
DeviceStorageFile::CreateUnique(const nsAString& aStorageType,
                                const nsAString& aStorageName,
                                nsAString& aFileName,
                                uint32_t aFileType,
                                uint32_t aFileAttributes)
{
  nsRefPtr<DeviceStorageFile> dsf =
    new DeviceStorageFile(aStorageType, aStorageName, aFileName);
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
  if (!mFile) {
    return NS_ERROR_FAILURE;
  }
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

  if (!mFile) {
    return NS_ERROR_FAILURE;
  }
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

  if (!mFile) {
    return NS_ERROR_FAILURE;
  }

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
  if (!mFile) {
    return;
  }
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

  DeviceStorageTypeChecker* typeChecker
    = DeviceStorageTypeChecker::CreateOrGet();
  MOZ_ASSERT(typeChecker);

  nsCOMPtr<nsIDirectoryEnumerator> files = do_QueryInterface(e);
  nsCOMPtr<nsIFile> f;

  while (NS_SUCCEEDED(files->GetNextFile(getter_AddRefs(f))) && f) {

    bool isFile;
    f->IsFile(&isFile);

    if (isFile) {
      PRTime msecs;
      f->GetLastModifiedTime(&msecs);

      if (msecs < aSince) {
        continue;
      }
    }

    bool isDir;
    f->IsDirectory(&isDir);

    nsString fullpath;
    nsresult rv = f->GetPath(fullpath);
    if (NS_FAILED(rv)) {
      continue;
    }

    if (isFile && !typeChecker->Check(mStorageType, fullpath)) {
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
DeviceStorageFile::GetStorageFreeSpace(int64_t* aSoFar)
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
  if (!typeChecker || !mFile) {
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
  if (!typeChecker || !mFile) {
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
  if (!typeChecker || !mFile) {
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
  if (!typeChecker || !mFile) {
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
  if (!typeChecker || !mFile) {
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

void
nsDOMDeviceStorage::SetRootDirectoryForType(const nsAString& aStorageType,
                                            const nsAString& aStorageName)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIFile> f;
  DeviceStorageFile::GetRootDirectoryForType(aStorageType,
                                             aStorageName,
                                             getter_AddRefs(f));
  mRootDirectory = f;
  mStorageType = aStorageType;
  mStorageName = aStorageName;
}

nsDOMDeviceStorageCursor::nsDOMDeviceStorageCursor(nsIGlobalObject* aGlobal,
                                                   DeviceStorageCursorRequest* aRequest)
  : DOMCursor(aGlobal, nullptr)
  , mOkToCallContinue(false)
  , mRequest(aRequest)
{
}

nsDOMDeviceStorageCursor::~nsDOMDeviceStorageCursor()
{
}

void
nsDOMDeviceStorageCursor::FireSuccess(JS::Handle<JS::Value> aResult)
{
  mOkToCallContinue = true;
  DOMCursor::FireSuccess(aResult);
}

void
nsDOMDeviceStorageCursor::FireDone()
{
  mRequest = nullptr;
  DOMCursor::FireDone();
}

void
nsDOMDeviceStorageCursor::FireError(const nsString& aReason)
{
  mOkToCallContinue = false;
  mRequest = nullptr;

  if (!mResult.isUndefined()) {
    // If we previously succeeded, we cannot fail without
    // clearing the last result.
    mResult.setUndefined();
    mDone = false;
  }

  DOMCursor::FireError(aReason);
}

void
nsDOMDeviceStorageCursor::Continue(ErrorResult& aRv)
{
  if (!mOkToCallContinue || !mRequest) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  if (!mResult.isUndefined()) {
    // We call onsuccess multiple times. Clear the last
    // result.
    mResult.setUndefined();
    mDone = false;
  }

  mOkToCallContinue = false;
  aRv = mRequest->Continue();
}

DeviceStorageRequest::DeviceStorageRequest()
  : mId(DeviceStorageRequestManager::INVALID_ID)
  , mAccess(DEVICE_STORAGE_ACCESS_UNDEFINED)
  , mSendToParent(true)
  , mUseMainThread(false)
  , mUseStreamTransport(false)
  , mCheckFile(false)
  , mCheckBlob(false)
  , mMultipleResolve(false)
  , mPermissionCached(true)
{
  DS_LOG_DEBUG("%p", this);
}

DeviceStorageRequest::~DeviceStorageRequest()
{
  DS_LOG_DEBUG("%p", this);
  if (mId != DeviceStorageRequestManager::INVALID_ID) {
    /* Cursors may be freed without completing if the caller does not
       call continue until there is no data left. */
    MOZ_ASSERT(mMultipleResolve, "Still has valid ID but request being freed!");
    Reject(POST_ERROR_EVENT_UNKNOWN);
  }
}

void
DeviceStorageRequest::Initialize(DeviceStorageRequestManager* aManager,
                                 DeviceStorageFile* aFile,
                                 uint32_t aId)
{
  DS_LOG_DEBUG("%p manages %p", aManager, this);
  mManager = aManager;
  mFile = aFile;
  mId = aId;
  MOZ_ASSERT(mManager);
  MOZ_ASSERT(mFile);
  MOZ_ASSERT(mId != DeviceStorageRequestManager::INVALID_ID);
}

void
DeviceStorageRequest::Initialize(DeviceStorageRequestManager* aManager,
                                 DeviceStorageFile* aFile,
                                 uint32_t aRequest,
                                 BlobImpl* aBlob)
{
  Initialize(aManager, aFile, aRequest);
  mBlob = aBlob;
  mCheckBlob = true;
  mCheckFile = true;
  MOZ_ASSERT(mBlob);
}

void
DeviceStorageRequest::Initialize(DeviceStorageRequestManager* aManager,
                                 DeviceStorageFile* aFile,
                                 uint32_t aRequest,
                                 DeviceStorageFileDescriptor* aDSFileDescriptor)
{
  Initialize(aManager, aFile, aRequest);
  mDSFileDescriptor = aDSFileDescriptor;
  MOZ_ASSERT(mDSFileDescriptor);
}

DeviceStorageAccessType
DeviceStorageRequest::GetAccess() const
{
  return mAccess;
}

void
DeviceStorageRequest::GetStorageType(nsAString& aType) const
{
  aType = mFile->mStorageType;
}

nsresult
DeviceStorageRequest::Cancel()
{
  return Reject(POST_ERROR_EVENT_PERMISSION_DENIED);
}

nsresult
DeviceStorageRequest::Allow()
{
  if (mUseMainThread && !NS_IsMainThread()) {
    nsRefPtr<DeviceStorageRequest> self = this;
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([self] () -> void
    {
      self->Allow();
    });
    return NS_DispatchToMainThread(r);
  }

  nsresult rv = AllowInternal();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    const char *reason;
    switch (rv) {
      case NS_ERROR_ILLEGAL_VALUE:
        reason = POST_ERROR_EVENT_ILLEGAL_TYPE;
        break;
      case NS_ERROR_DOM_SECURITY_ERR:
        reason = POST_ERROR_EVENT_PERMISSION_DENIED;
        break;
      default:
        reason = POST_ERROR_EVENT_UNKNOWN;
        break;
    }
    return Reject(reason);
  }
  return rv;
}

DeviceStorageFile*
DeviceStorageRequest::GetFile() const
{
  MOZ_ASSERT(mFile);
  return mFile;
}

DeviceStorageFileDescriptor*
DeviceStorageRequest::GetFileDescriptor() const
{
  MOZ_ASSERT(mDSFileDescriptor);
  return mDSFileDescriptor;
}

DeviceStorageRequestManager*
DeviceStorageRequest::GetManager() const
{
  return mManager;
}

nsresult
DeviceStorageRequest::Prepare()
{
  return NS_OK;
}

nsresult
DeviceStorageRequest::CreateSendParams(DeviceStorageParams& aParams)
{
  MOZ_ASSERT_UNREACHABLE("Cannot send to parent, missing param creator");
  return NS_ERROR_UNEXPECTED;
}

nsresult
DeviceStorageRequest::AllowInternal()
{
  MOZ_ASSERT(mManager->IsOwningThread() || NS_IsMainThread());

  nsresult rv = Prepare();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  DeviceStorageTypeChecker* typeChecker
    = DeviceStorageTypeChecker::CreateOrGet();
  if (!typeChecker) {
    return NS_ERROR_UNEXPECTED;
  }
  if (mCheckBlob && (!mBlob ||
      !typeChecker->Check(mFile->mStorageType, mBlob))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  if (mCheckFile && (!mFile->mFile ||
      !typeChecker->Check(mFile->mStorageType, mFile->mFile))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  mSendToParent = mSendToParent && !XRE_IsParentProcess();
  if (mSendToParent) {
    return SendToParentProcess();
  }

  if (mUseStreamTransport) {
    DS_LOG_INFO("run stream transport %u", mId);
    nsCOMPtr<nsIEventTarget> target
      = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
    MOZ_ASSERT(target);
    return target->Dispatch(this, NS_DISPATCH_NORMAL);
  }

  DS_LOG_INFO("run %u", mId);
  return Run();
}

nsresult
DeviceStorageRequest::SendToParentProcess()
{
  // PContent can only be used on the main thread
  if (!NS_IsMainThread()) {
    nsRefPtr<DeviceStorageRequest> self = this;
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([self] () -> void
    {
      nsresult rv = self->SendToParentProcess();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        self->Reject(POST_ERROR_EVENT_UNKNOWN);
      }
    });
    return NS_DispatchToMainThread(r);
  }

  MOZ_ASSERT(NS_IsMainThread());
  DS_LOG_INFO("request parent %u", mId);

  DeviceStorageParams params;
  nsresult rv = CreateSendParams(params);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_UNEXPECTED;
  }

  PDeviceStorageRequestChild* child = new DeviceStorageRequestChild(this);
  ContentChild::GetSingleton()
    ->SendPDeviceStorageRequestConstructor(child, params);
  return NS_OK;
}

DeviceStorageCursorRequest::DeviceStorageCursorRequest()
  : mIndex(0)
  , mSince(0)
{
  mAccess = DEVICE_STORAGE_ACCESS_READ;
  mUseStreamTransport = true;
  mMultipleResolve = true;
  DS_LOG_INFO("");
}

void
DeviceStorageCursorRequest::Initialize(DeviceStorageRequestManager* aManager,
                                       DeviceStorageFile* aFile,
                                       uint32_t aRequest,
                                       PRTime aSince)
{
  Initialize(aManager, aFile, aRequest);
  mStorageType = mFile->mStorageType;
  mSince = aSince;
}

void
DeviceStorageCursorRequest::AddFiles(size_t aSize)
{
  mFiles.SetCapacity(mFiles.Length() + aSize);
}

void
DeviceStorageCursorRequest::AddFile(already_AddRefed<DeviceStorageFile> aFile)
{
  mFiles.AppendElement(aFile);
}

nsresult
DeviceStorageCursorRequest::SendContinueToParentProcess()
{
  if (!NS_IsMainThread()) {
    nsRefPtr<DeviceStorageCursorRequest> self = this;
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([self] () -> void
    {
      self->SendContinueToParentProcess();
    });
    return NS_DispatchToMainThread(r);
  }

  MOZ_ASSERT(NS_IsMainThread());
  DS_LOG_INFO("request parent %u", mId);

  DeviceStorageRequestChild* child
    = new DeviceStorageRequestChild(this);
  DeviceStorageGetParams params(mStorageType,
                                mFile->mStorageName,
                                mFile->mRootDir,
                                mFile->mPath);
  ContentChild::GetSingleton()
    ->SendPDeviceStorageRequestConstructor(child, params);
  return NS_OK;
}

nsresult
DeviceStorageCursorRequest::Continue()
{
  if (!NS_IsMainThread()) {
    /* The MIME service can only be accessed from the main thread */
    nsRefPtr<DeviceStorageCursorRequest> self = this;
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([self] () -> void
    {
      self->Continue();
    });
    nsresult rv = NS_DispatchToMainThread(r);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return Reject(POST_ERROR_EVENT_UNKNOWN);
    }
    return rv;
  }

  DS_LOG_INFO("%u", mId);

  nsRefPtr<DeviceStorageFile> file;
  while (!file && mIndex < mFiles.Length()) {
    file = mFiles[mIndex].forget();
    ++mIndex;
  }

  if (!file) {
    // No more files remaining, complete cursor
    return Resolve();
  }

  file->CalculateMimeType();
  if (XRE_IsParentProcess()) {
    return Resolve(file);
  }

  mFile = file;

  nsresult rv = SendContinueToParentProcess();
  if (NS_FAILED(rv)) {
    return Reject(POST_ERROR_EVENT_UNKNOWN);
  }
  return rv;
}

NS_IMETHODIMP
DeviceStorageCursorRequest::Run()
{
  if (mFile->mFile) {
    bool check;
    mFile->mFile->IsDirectory(&check);
    if (!check) {
      return Reject(POST_ERROR_EVENT_FILE_NOT_ENUMERABLE);
    }
  }

  mFile->CollectFiles(mFiles, mSince);
  return Continue();
}

nsresult
DeviceStorageCursorRequest::CreateSendParams(DeviceStorageParams& aParams)
{
  DeviceStorageEnumerationParams params(mFile->mStorageType,
                                        mFile->mStorageName,
                                        mFile->mRootDir,
                                        mSince);
  aParams = params;
  return NS_OK;
}

class DeviceStorageCreateFdRequest final
  : public DeviceStorageRequest
{
public:
  DeviceStorageCreateFdRequest()
  {
    mAccess = DEVICE_STORAGE_ACCESS_CREATE;
    mUseStreamTransport = true;
    mCheckFile = true;
    DS_LOG_INFO("");
  }

  NS_IMETHOD Run() override
  {
    nsString fullPath;
    mFile->GetFullPath(fullPath);
    MOZ_ASSERT(!fullPath.IsEmpty());

    bool check = false;
    mFile->mFile->Exists(&check);
    if (check) {
      return Reject(POST_ERROR_EVENT_FILE_EXISTS);
    }

    nsresult rv = mFile->CreateFileDescriptor(
                  mDSFileDescriptor->mFileDescriptor);

    if (NS_FAILED(rv)) {
      mFile->mFile->Remove(false);
      return Reject(POST_ERROR_EVENT_UNKNOWN);
    }

    return Resolve(fullPath);
  }

protected:
  nsresult CreateSendParams(DeviceStorageParams& aParams) override
  {
    DeviceStorageCreateFdParams params;
    params.type() = mFile->mStorageType;
    params.storageName() = mFile->mStorageName;
    params.relpath() = mFile->mPath;
    aParams = params;

    mFile->Dump("DeviceStorageCreateFdParams");
    return NS_OK;
  }
};

class DeviceStorageCreateRequest final
  : public DeviceStorageRequest
{
public:
  using DeviceStorageRequest::Initialize;

  DeviceStorageCreateRequest()
  {
    mAccess = DEVICE_STORAGE_ACCESS_CREATE;
    mUseStreamTransport = true;
    DS_LOG_INFO("");
  }

  NS_IMETHOD Run() override
  {
    ErrorResult rv;
    nsCOMPtr<nsIInputStream> stream;
    mBlob->GetInternalStream(getter_AddRefs(stream), rv);
    if (NS_WARN_IF(rv.Failed())) {
      return Reject(POST_ERROR_EVENT_UNKNOWN);
    }

    bool check = false;
    mFile->mFile->Exists(&check);
    if (check) {
      return Reject(POST_ERROR_EVENT_FILE_EXISTS);
    }

    rv = mFile->Write(stream);
    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
      mFile->mFile->Remove(false);
      return Reject(POST_ERROR_EVENT_UNKNOWN);
    }

    nsString fullPath;
    mFile->GetFullPath(fullPath);
    return Resolve(fullPath);
  }

  void Initialize(DeviceStorageRequestManager* aManager,
                  DeviceStorageFile* aFile,
                  uint32_t aRequest) override
  {
    DeviceStorageRequest::Initialize(aManager, aFile, aRequest);
    mUseMainThread = aFile->mPath.IsEmpty();
  }

protected:
  nsresult Prepare() override
  {
    if (!mFile->mPath.IsEmpty()) {
      // Checks have already been performed when request was created
      return NS_OK;
    }

    MOZ_ASSERT(NS_IsMainThread());

    nsCOMPtr<nsIMIMEService> mimeSvc = do_GetService(NS_MIMESERVICE_CONTRACTID);
    if (!mimeSvc) {
      return NS_ERROR_FAILURE;
    }

    // if mimeType or extension are null, the request will be rejected
    // in DeviceStorageRequest::AllowInternal when the type checker
    // verifies the file path
    nsString mimeType;
    mBlob->GetType(mimeType);

    nsCString extension;
    mimeSvc->GetPrimaryExtension(NS_LossyConvertUTF16toASCII(mimeType),
                                 EmptyCString(), extension);

    char buffer[32];
    NS_MakeRandomString(buffer, ArrayLength(buffer) - 1);

    nsAutoString path;
    path.AssignLiteral(buffer);
    path.Append('.');
    path.AppendASCII(extension.get());

    nsRefPtr<DeviceStorageFile> file
      = DeviceStorageFile::CreateUnique(mFile->mStorageType,
                                        mFile->mStorageName, path,
                                        nsIFile::NORMAL_FILE_TYPE, 00600);
    if (!file) {
      return NS_ERROR_FAILURE;
    }
    if (!file->IsSafePath()) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }

    mFile = file.forget();
    return NS_OK;
  }

  nsresult CreateSendParams(DeviceStorageParams& aParams) override
  {
    BlobChild* actor
      = ContentChild::GetSingleton()->GetOrCreateActorForBlobImpl(mBlob);
    if (!actor) {
      return NS_ERROR_FAILURE;
    }

    DeviceStorageAddParams params;
    params.blobChild() = actor;
    params.type() = mFile->mStorageType;
    params.storageName() = mFile->mStorageName;
    params.relpath() = mFile->mPath;
    aParams = params;
    return NS_OK;
  }
};

class DeviceStorageAppendRequest final
  : public DeviceStorageRequest
{
public:
  DeviceStorageAppendRequest()
  {
    mAccess = DEVICE_STORAGE_ACCESS_WRITE;
    mUseStreamTransport = true;
    DS_LOG_INFO("");
  }

  NS_IMETHOD Run() override
  {
    ErrorResult rv;
    nsCOMPtr<nsIInputStream> stream;
    mBlob->GetInternalStream(getter_AddRefs(stream), rv);
    if (NS_WARN_IF(rv.Failed())) {
      return Reject(POST_ERROR_EVENT_UNKNOWN);
    }

    bool check = false;
    mFile->mFile->Exists(&check);
    if (!check) {
      return Reject(POST_ERROR_EVENT_FILE_DOES_NOT_EXIST);
    }

    rv = mFile->Append(stream);
    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
      return Reject(POST_ERROR_EVENT_UNKNOWN);
    }

    nsString fullPath;
    mFile->GetFullPath(fullPath);
    return Resolve(fullPath);
  }

protected:
  nsresult CreateSendParams(DeviceStorageParams& aParams) override
  {
    BlobChild* actor
      = ContentChild::GetSingleton()->GetOrCreateActorForBlobImpl(mBlob);
    if (!actor) {
      return NS_ERROR_FAILURE;
    }

    DeviceStorageAppendParams params;
    params.blobChild() = actor;
    params.type() = mFile->mStorageType;
    params.storageName() = mFile->mStorageName;
    params.relpath() = mFile->mPath;
    aParams = params;
    return NS_OK;
  }
};

class DeviceStorageOpenRequest final
  : public DeviceStorageRequest
{
public:
  using DeviceStorageRequest::Initialize;

  DeviceStorageOpenRequest()
  {
    mUseMainThread = true;
    mUseStreamTransport = true;
    mCheckFile = true;
    DS_LOG_INFO("");
  }

  void Initialize(DeviceStorageRequestManager* aManager,
                  DeviceStorageFile* aFile,
                  uint32_t aRequest) override
  {
    DeviceStorageRequest::Initialize(aManager, aFile, aRequest);
    mAccess = mFile->mEditable ? DEVICE_STORAGE_ACCESS_WRITE
                               : DEVICE_STORAGE_ACCESS_READ;
  }

  NS_IMETHOD Run() override
  {
    if (!mFile->mEditable) {
      bool check = false;
      mFile->mFile->Exists(&check);
      if (!check) {
        return Reject(POST_ERROR_EVENT_FILE_DOES_NOT_EXIST);
      }
    }

    nsresult rv = mFile->CalculateSizeAndModifiedDate();
    if (NS_FAILED(rv)) {
      return Reject(POST_ERROR_EVENT_UNKNOWN);
    }

    return Resolve(mFile);
  }

protected:
  nsresult Prepare() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    mFile->CalculateMimeType();
    return NS_OK;
  }

  nsresult CreateSendParams(DeviceStorageParams& aParams) override
  {
    DeviceStorageGetParams params(mFile->mStorageType,
                                  mFile->mStorageName,
                                  mFile->mRootDir,
                                  mFile->mPath);
    aParams = params;
    return NS_OK;
  }
};

class DeviceStorageDeleteRequest final
  : public DeviceStorageRequest
{
public:
  DeviceStorageDeleteRequest()
  {
    mAccess = DEVICE_STORAGE_ACCESS_WRITE;
    mUseStreamTransport = true;
    mCheckFile = true;
    DS_LOG_INFO("");
  }

  NS_IMETHOD Run() override
  {
    mFile->Remove();
    bool check = false;
    mFile->mFile->Exists(&check);
    if (check) {
      return Reject(POST_ERROR_EVENT_FILE_DOES_NOT_EXIST);
    }

    nsString fullPath;
    mFile->GetFullPath(fullPath);
    return Resolve(fullPath);
  }

protected:
  nsresult CreateSendParams(DeviceStorageParams& aParams) override
  {
    DeviceStorageDeleteParams params(mFile->mStorageType,
                                     mFile->mStorageName,
                                     mFile->mPath);
    aParams = params;
    return NS_OK;
  }
};

class DeviceStorageFreeSpaceRequest final
  : public DeviceStorageRequest
{
public:
  DeviceStorageFreeSpaceRequest()
  {
    mAccess = DEVICE_STORAGE_ACCESS_READ;
    mUseStreamTransport = true;
    DS_LOG_INFO("");
  }

  NS_IMETHOD Run() override
  {
    int64_t freeSpace = 0;
    if (mFile) {
      mFile->GetStorageFreeSpace(&freeSpace);
    }
    return Resolve(static_cast<uint64_t>(freeSpace));
  }

protected:
  nsresult CreateSendParams(DeviceStorageParams& aParams) override
  {
    DeviceStorageFreeSpaceParams params(mFile->mStorageType,
                                        mFile->mStorageName);
    aParams = params;
    return NS_OK;
  }
};

class DeviceStorageUsedSpaceRequest final
  : public DeviceStorageRequest
{
public:
  DeviceStorageUsedSpaceRequest()
  {
    mAccess = DEVICE_STORAGE_ACCESS_READ;
    DS_LOG_INFO("");
  }

  NS_IMETHOD Run() override
  {
    if (mManager->IsOwningThread()) {
      // this needs to be dispatched to only one (1)
      // thread or we will do more work than required.
      DeviceStorageUsedSpaceCache* usedSpaceCache
        = DeviceStorageUsedSpaceCache::CreateOrGet();
      MOZ_ASSERT(usedSpaceCache);
      usedSpaceCache->Dispatch(this);
      return NS_OK;
    }

    uint64_t picturesUsage = 0, videosUsage = 0,
             musicUsage = 0, totalUsage = 0;
    mFile->AccumDiskUsage(&picturesUsage, &videosUsage,
                          &musicUsage, &totalUsage);

    const nsString& type = mFile->mStorageType;
    if (type.EqualsLiteral(DEVICESTORAGE_PICTURES)) {
      totalUsage = picturesUsage;
    } else if (type.EqualsLiteral(DEVICESTORAGE_VIDEOS)) {
      totalUsage = videosUsage;
    } else if (type.EqualsLiteral(DEVICESTORAGE_MUSIC)) {
      totalUsage = musicUsage;
    }
    return Resolve(totalUsage);
  }

protected:
  nsresult CreateSendParams(DeviceStorageParams& aParams) override
  {
    DeviceStorageUsedSpaceParams params(mFile->mStorageType,
                                        mFile->mStorageName);
    aParams = params;
    return NS_OK;
  }
};

class DeviceStorageAvailableRequest final
  : public DeviceStorageRequest
{
public:
  DeviceStorageAvailableRequest()
  {
    mAccess = DEVICE_STORAGE_ACCESS_READ;
    DS_LOG_INFO("");
  }

  NS_IMETHOD Run() override
  {
    nsString state = NS_LITERAL_STRING("unavailable");
    if (mFile) {
      mFile->GetStatus(state);
    }
    return Resolve(state);
  }

protected:
  nsresult CreateSendParams(DeviceStorageParams& aParams) override
  {
    DeviceStorageAvailableParams params(mFile->mStorageType,
                                        mFile->mStorageName);
    aParams = params;
    return NS_OK;
  }
};

class DeviceStorageStatusRequest final
  : public DeviceStorageRequest
{
public:
  DeviceStorageStatusRequest()
  {
    mAccess = DEVICE_STORAGE_ACCESS_READ;
    DS_LOG_INFO("");
  }

  NS_IMETHOD Run() override
  {
    nsString state = NS_LITERAL_STRING("undefined");
    if (mFile) {
      mFile->GetStorageStatus(state);
    }
    return Resolve(state);
  }

protected:
  nsresult CreateSendParams(DeviceStorageParams& aParams) override
  {
    DeviceStorageStatusParams params(mFile->mStorageType,
                                     mFile->mStorageName);
    aParams = params;
    return NS_OK;
  }
};

class DeviceStorageWatchRequest final
  : public DeviceStorageRequest
{
public:
  DeviceStorageWatchRequest()
  {
    mAccess = DEVICE_STORAGE_ACCESS_READ;
    mSendToParent = false;
    DS_LOG_INFO("");
  }

  NS_IMETHOD Run() override
  {
    return Resolve();
  }
};

class DeviceStorageFormatRequest final
  : public DeviceStorageRequest
{
public:
  DeviceStorageFormatRequest()
  {
    mAccess = DEVICE_STORAGE_ACCESS_WRITE;
    DS_LOG_INFO("");
  }

  NS_IMETHOD Run() override
  {
    nsString state = NS_LITERAL_STRING("unavailable");
    if (mFile) {
      mFile->DoFormat(state);
    }
    return Resolve(state);
  }

protected:
  nsresult CreateSendParams(DeviceStorageParams& aParams) override
  {
    DeviceStorageFormatParams params(mFile->mStorageType,
                                     mFile->mStorageName);
    aParams = params;
    return NS_OK;
  }
};

class DeviceStorageMountRequest final
  : public DeviceStorageRequest
{
public:
  DeviceStorageMountRequest()
  {
    mAccess = DEVICE_STORAGE_ACCESS_WRITE;
    DS_LOG_INFO("");
  }

  NS_IMETHOD Run() override
  {
    nsString state = NS_LITERAL_STRING("unavailable");
    if (mFile) {
      mFile->DoMount(state);
    }
    return Resolve(state);
  }

protected:
  nsresult CreateSendParams(DeviceStorageParams& aParams) override
  {
    DeviceStorageMountParams params(mFile->mStorageType,
                                    mFile->mStorageName);
    aParams = params;
    return NS_OK;
  }
};

class DeviceStorageUnmountRequest final
  : public DeviceStorageRequest
{
public:
  DeviceStorageUnmountRequest()
  {
    mAccess = DEVICE_STORAGE_ACCESS_WRITE;
    DS_LOG_INFO("");
  }

  NS_IMETHOD Run() override
  {
    nsString state = NS_LITERAL_STRING("unavailable");
    if (mFile) {
      mFile->DoUnmount(state);
    }
    return Resolve(state);
  }

protected:
  nsresult CreateSendParams(DeviceStorageParams& aParams) override
  {
    DeviceStorageUnmountParams params(mFile->mStorageType,
                                      mFile->mStorageName);
    aParams = params;
    return NS_OK;
  }
};

class DeviceStoragePermissionCheck final
  : public nsIContentPermissionRequest
  , public nsIRunnable
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(DeviceStoragePermissionCheck,
                                           nsIContentPermissionRequest)

  DeviceStoragePermissionCheck(DeviceStorageRequest* aRequest,
                               uint64_t aWindowID,
                               const PrincipalInfo &aPrincipalInfo)
    : mRequest(aRequest)
    , mWindowID(aWindowID)
    , mPrincipalInfo(aPrincipalInfo)
  {
    MOZ_ASSERT(mRequest);
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (DeviceStorageStatics::IsPromptTesting()) {
      return Allow(JS::UndefinedHandleValue);
    }

    mWindow = nsGlobalWindow::GetInnerWindowWithId(mWindowID);
    if (NS_WARN_IF(!mWindow)) {
      return Cancel();
    }

    nsresult rv;
    mPrincipal = PrincipalInfoToPrincipal(mPrincipalInfo, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return Cancel();
    }

    mRequester = new nsContentPermissionRequester(mWindow);
    return nsContentPermissionUtils::AskPermission(this, mWindow);
  }

  NS_IMETHOD Cancel() override
  {
    return Resolve(false);
  }

  NS_IMETHOD Allow(JS::HandleValue aChoices) override
  {
    MOZ_ASSERT(aChoices.isUndefined());
    return Resolve(true);
  }

  NS_IMETHODIMP GetTypes(nsIArray** aTypes) override
  {
    nsString storageType;
    mRequest->GetStorageType(storageType);
    nsCString type;
    nsresult rv =
      DeviceStorageTypeChecker::GetPermissionForType(storageType, type);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCString access;
    rv = DeviceStorageTypeChecker::GetAccessForIndex(mRequest->GetAccess(),
                                                     access);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsTArray<nsString> emptyOptions;
    return nsContentPermissionUtils::CreatePermissionArray(type, access, emptyOptions, aTypes);
  }

  NS_IMETHOD GetRequester(nsIContentPermissionRequester** aRequester) override
  {
    NS_ENSURE_ARG_POINTER(aRequester);

    nsCOMPtr<nsIContentPermissionRequester> requester = mRequester;
    requester.forget(aRequester);
    return NS_OK;
  }

  NS_IMETHOD GetPrincipal(nsIPrincipal * *aRequestingPrincipal) override
  {
    NS_IF_ADDREF(*aRequestingPrincipal = mPrincipal);
    return NS_OK;
  }

  NS_IMETHOD GetWindow(nsIDOMWindow * *aRequestingWindow) override
  {
    NS_IF_ADDREF(*aRequestingWindow = mWindow);
    return NS_OK;
  }

  NS_IMETHOD GetElement(nsIDOMElement * *aRequestingElement) override
  {
    *aRequestingElement = nullptr;
    return NS_OK;
  }

private:
  nsresult Resolve(bool aResolve)
  {
    mRequest->GetManager()->StorePermission(mRequest->GetAccess(), aResolve);
    mRequest->PermissionCacheMissed();
    if (aResolve) {
      return mRequest->Allow();
    }
    return mRequest->Cancel();
  }

  virtual ~DeviceStoragePermissionCheck()
  { }

  nsRefPtr<DeviceStorageRequest> mRequest;
  uint64_t mWindowID;
  PrincipalInfo mPrincipalInfo;
  nsCOMPtr<nsPIDOMWindow> mWindow;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsIContentPermissionRequester> mRequester;
};

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DeviceStoragePermissionCheck)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContentPermissionRequest)
  NS_INTERFACE_MAP_ENTRY(nsIContentPermissionRequest)
  NS_INTERFACE_MAP_ENTRY(nsIRunnable)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DeviceStoragePermissionCheck)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DeviceStoragePermissionCheck)

NS_IMPL_CYCLE_COLLECTION(DeviceStoragePermissionCheck,
                         mWindow)


NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMDeviceStorage)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  /* nsISupports is an ambiguous base of nsDOMDeviceStorage
     so we have to work around that. */
  if ( aIID.Equals(NS_GET_IID(nsDOMDeviceStorage)) )
    foundInterface = static_cast<nsISupports*>(static_cast<void*>(this));
  else
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(nsDOMDeviceStorage, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(nsDOMDeviceStorage, DOMEventTargetHelper)

int nsDOMDeviceStorage::sInstanceCount = 0;

nsDOMDeviceStorage::nsDOMDeviceStorage(nsPIDOMWindow* aWindow)
  : DOMEventTargetHelper(aWindow)
  , mIsShareable(false)
  , mIsRemovable(false)
  , mInnerWindowID(0)
  , mOwningThread(NS_GetCurrentThread())
{
  MOZ_ASSERT(NS_IsMainThread()); // worker support incomplete
  sInstanceCount++;
  DS_LOG_DEBUG("%p (%d)", this, sInstanceCount);
}

nsresult
nsDOMDeviceStorage::CheckPermission(DeviceStorageRequest* aRequest)
{
  MOZ_ASSERT(mManager);
  uint32_t cache = mManager->CheckPermission(aRequest->GetAccess());
  switch (cache) {
    case nsIPermissionManager::ALLOW_ACTION:
      return aRequest->Allow();
    case nsIPermissionManager::DENY_ACTION:
      return aRequest->Cancel();
    case nsIPermissionManager::PROMPT_ACTION:
    default:
    {
      nsCOMPtr<nsIThread> mainThread;
      nsresult rv = NS_GetMainThread(getter_AddRefs(mainThread));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return aRequest->Reject(POST_ERROR_EVENT_UNKNOWN);
      }

      /* We need to do a bit of a song and dance here to release the object
         because while we can initially increment the ownership count (no one
         else is using it), we cannot safely decrement after dispatching because
         it uses cycle collection and requires the main thread to free it. */
      nsCOMPtr<nsIRunnable> r
        = new DeviceStoragePermissionCheck(aRequest, mInnerWindowID,
                                           *mPrincipalInfo);
      rv = mainThread->Dispatch(r, NS_DISPATCH_NORMAL);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        rv = aRequest->Reject(POST_ERROR_EVENT_UNKNOWN);
      }
      NS_ProxyRelease(mainThread, r.forget().take());
      return rv;
    }
  }
}

bool
nsDOMDeviceStorage::IsOwningThread()
{
  bool owner = false;
  mOwningThread->IsOnCurrentThread(&owner);
  return owner;
}

nsresult
nsDOMDeviceStorage::DispatchToOwningThread(nsIRunnable* aRunnable)
{
  return mOwningThread->Dispatch(aRunnable, NS_DISPATCH_NORMAL);
}

/* virtual */ JSObject*
nsDOMDeviceStorage::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return DeviceStorageBinding::Wrap(aCx, this, aGivenProto);
}

nsresult
nsDOMDeviceStorage::Init(nsPIDOMWindow* aWindow, const nsAString &aType,
                         const nsAString &aVolName)
{
  MOZ_ASSERT(aWindow);
  mInnerWindowID = aWindow->WindowID();

  SetRootDirectoryForType(aType, aVolName);
  if (!mRootDirectory) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv;
  DeviceStorageStatics::AddListener(this);
  if (!mStorageName.IsEmpty()) {
    mIsDefaultLocation = Default();

#ifdef MOZ_WIDGET_GONK
    if (DeviceStorageTypeChecker::IsVolumeBased(mStorageType)) {
      nsCOMPtr<nsIVolumeService> vs = do_GetService(NS_VOLUMESERVICE_CONTRACTID);
      if (NS_WARN_IF(!vs)) {
        return NS_ERROR_FAILURE;
      }
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
      bool isRemovable;
      rv = vol->GetIsHotSwappable(&isRemovable);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      mIsRemovable = isRemovable;
    }
#endif
  }

  nsCOMPtr<nsIPrincipal> principal;
  rv = CheckPrincipal(aWindow, aType.EqualsLiteral(DEVICESTORAGE_APPS), getter_AddRefs(principal));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mPrincipalInfo = new PrincipalInfo();
  rv = PrincipalToPrincipalInfo(principal, mPrincipalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mManager = new DeviceStorageRequestManager();
  DS_LOG_DEBUG("%p owns %p", this, mManager.get());
  return NS_OK;
}

nsDOMDeviceStorage::~nsDOMDeviceStorage()
{
  DS_LOG_DEBUG("%p (%d)", this, sInstanceCount);
  MOZ_ASSERT(IsOwningThread());
  sInstanceCount--;
  DeviceStorageStatics::RemoveListener(this);
}

// static
nsresult
nsDOMDeviceStorage::CheckPrincipal(nsPIDOMWindow* aWindow, bool aIsAppsStorage, nsIPrincipal** aPrincipal)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  // Grab the principal of the document
  nsCOMPtr<nsIDocument> doc = aWindow->GetDoc();
  if (!doc) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIPrincipal> principal = doc->NodePrincipal();

  // the 'apps' type is special.  We only want this exposed
  // if the caller has the "webapps-manage" permission.
  if (aIsAppsStorage) {
    nsCOMPtr<nsIPermissionManager> permissionManager
      = services::GetPermissionManager();
    NS_ENSURE_TRUE(permissionManager, NS_ERROR_FAILURE);

    uint32_t permission;
    nsresult rv
      = permissionManager->TestPermissionFromPrincipal(principal,
                                                       "webapps-manage",
                                                       &permission);

    if (NS_FAILED(rv) || permission != nsIPermissionManager::ALLOW_ACTION) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  principal.forget(aPrincipal);
  return NS_OK;
}

void
nsDOMDeviceStorage::Shutdown()
{
  MOZ_ASSERT(IsOwningThread());

  if (mManager) {
    mManager->Shutdown();
    mManager = nullptr;
  }

  if (mFileSystem) {
    mFileSystem->Shutdown();
    mFileSystem = nullptr;
  }

  DeviceStorageStatics::RemoveListener(this);
}

StaticAutoPtr<nsTArray<nsString>> nsDOMDeviceStorage::sVolumeNameCache;

// static
void nsDOMDeviceStorage::InvalidateVolumeCaches()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Currently there is only the one volume cache. DeviceStorageAreaListener
  // calls this function any time it detects a volume was added or removed.

  sVolumeNameCache = nullptr;
}

// static
void
nsDOMDeviceStorage::GetOrderedVolumeNames(
  const nsAString& aType,
  nsDOMDeviceStorage::VolumeNameArray& aVolumeNames)
{
  if (!DeviceStorageTypeChecker::IsVolumeBased(aType)) {
    aVolumeNames.Clear();
    return;
  }
  GetOrderedVolumeNames(aVolumeNames);
}

// static
void
nsDOMDeviceStorage::GetOrderedVolumeNames(
  nsDOMDeviceStorage::VolumeNameArray& aVolumeNames)
{
  MOZ_ASSERT(NS_IsMainThread());

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
  GetDefaultStorageName(aType, storageName);

  nsRefPtr<nsDOMDeviceStorage> ds = new nsDOMDeviceStorage(aWin);
  if (NS_FAILED(ds->Init(aWin, aType, storageName))) {
    *aStore = nullptr;
    return;
  }
  ds.forget(aStore);
}

// static
void
nsDOMDeviceStorage::CreateDeviceStorageByNameAndType(
  nsPIDOMWindow* aWin,
  const nsAString& aName,
  const nsAString& aType,
  nsDOMDeviceStorage** aStore)
{
  if (!DeviceStorageTypeChecker::IsVolumeBased(aType)) {
    nsRefPtr<nsDOMDeviceStorage> storage = new nsDOMDeviceStorage(aWin);
    if (NS_FAILED(storage->Init(aWin, aType, EmptyString()))) {
      *aStore = nullptr;
      return;
    }
    NS_ADDREF(*aStore = storage.get());
    return;
  }

  nsRefPtr<nsDOMDeviceStorage> storage = GetStorageByNameAndType(aWin,
                                                                 aName,
                                                                 aType);
  if (!storage) {
    *aStore = nullptr;
    return;
  }
  NS_ADDREF(*aStore = storage.get());
}

bool
nsDOMDeviceStorage::Equals(nsPIDOMWindow* aWin,
                           const nsAString& aName,
                           const nsAString& aType)
{
  MOZ_ASSERT(aWin);

  return aWin && aWin->WindowID() == mInnerWindowID &&
         mStorageName.Equals(aName) &&
         mStorageType.Equals(aType);
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
  MOZ_ASSERT(IsOwningThread());

  nsRefPtr<nsDOMDeviceStorage> ds;

  if (mStorageName.Equals(aStorageName)) {
    ds = this;
    return ds.forget();
  }

  return GetStorageByNameAndType(GetOwner(), aStorageName, mStorageType);
}

// static
already_AddRefed<nsDOMDeviceStorage>
nsDOMDeviceStorage::GetStorageByNameAndType(nsPIDOMWindow* aWin,
                                            const nsAString& aStorageName,
                                            const nsAString& aType)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<nsDOMDeviceStorage> ds;

  VolumeNameArray volNames;
  GetOrderedVolumeNames(volNames);
  VolumeNameArray::size_type numVolumes = volNames.Length();
  VolumeNameArray::index_type i;
  for (i = 0; i < numVolumes; i++) {
    if (volNames[i].Equals(aStorageName)) {
      ds = new nsDOMDeviceStorage(aWin);
      nsresult rv = ds->Init(aWin, aType, aStorageName);
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
  if (!DeviceStorageTypeChecker::IsVolumeBased(aStorageType)) {
    // The storage name will be the empty string
    aStorageName.Truncate();
    return;
  }

  // See if the preferred volume is available.
  nsString prefStorageName;
  DeviceStorageStatics::GetWritableName(prefStorageName);

  if (!prefStorageName.IsEmpty()) {
    nsString status;
    nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(aStorageType,
                                                            prefStorageName);
    dsf->GetStorageStatus(status);

    if (!status.EqualsLiteral("NoMedia")) {
      aStorageName = prefStorageName;
      return;
    }
  }

  // If there is no preferred storage or preferred storage is not presented,
  // we'll use the first one (which should be sdcard).
  VolumeNameArray volNames;
  GetOrderedVolumeNames(volNames);
  if (volNames.Length() > 0) {
    aStorageName = volNames[0];
    // overwrite the value of "device.storage.writable.name"
    DeviceStorageStatics::SetWritableName(aStorageName);
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

already_AddRefed<DOMRequest>
nsDOMDeviceStorage::Add(Blob* aBlob, ErrorResult& aRv)
{
  nsString path;
  return AddOrAppendNamed(aBlob, path, true, aRv);
}

already_AddRefed<DOMRequest>
nsDOMDeviceStorage::AddNamed(Blob* aBlob, const nsAString& aPath,
                             ErrorResult& aRv)
{
  if (aPath.IsEmpty()) {
    aRv.Throw(NS_ERROR_ILLEGAL_VALUE);
    return nullptr;
  }
  return AddOrAppendNamed(aBlob, aPath, true, aRv);
}

already_AddRefed<DOMRequest>
nsDOMDeviceStorage::AppendNamed(Blob* aBlob, const nsAString& aPath,
                                ErrorResult& aRv)
{
  if (aPath.IsEmpty()) {
    aRv.Throw(NS_ERROR_ILLEGAL_VALUE);
    return nullptr;
  }
  return AddOrAppendNamed(aBlob, aPath, false, aRv);
}

uint32_t
nsDOMDeviceStorage::CreateDOMRequest(DOMRequest** aRequest, ErrorResult& aRv)
{
  if (!mManager) {
    DS_LOG_WARN("shutdown");
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return DeviceStorageRequestManager::INVALID_ID;
  }

  return mManager->Create(this, aRequest);
}

uint32_t
nsDOMDeviceStorage::CreateDOMCursor(DeviceStorageCursorRequest* aRequest, nsDOMDeviceStorageCursor** aCursor, ErrorResult& aRv)
{
  if (!mManager) {
    DS_LOG_WARN("shutdown");
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return DeviceStorageRequestManager::INVALID_ID;
  }

  return mManager->Create(this, aRequest, aCursor);
}

already_AddRefed<DOMRequest>
nsDOMDeviceStorage::CreateAndRejectDOMRequest(const char *aReason, ErrorResult& aRv)
{
  nsRefPtr<DOMRequest> request;
  uint32_t id = CreateDOMRequest(getter_AddRefs(request), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  aRv = mManager->Reject(id, aReason);
  return request.forget();
}

already_AddRefed<DOMRequest>
nsDOMDeviceStorage::AddOrAppendNamed(Blob* aBlob, const nsAString& aPath,
                                     bool aCreate, ErrorResult& aRv)
{
  MOZ_ASSERT(IsOwningThread());
  MOZ_ASSERT(aCreate || !aPath.IsEmpty());

  // if the blob is null here, bail
  if (!aBlob) {
    return nullptr;
  }

  nsCOMPtr<nsIRunnable> r;

  if (IsFullPath(aPath)) {
    nsString storagePath;
    nsRefPtr<nsDOMDeviceStorage> ds = GetStorage(aPath, storagePath);
    if (!ds) {
      return CreateAndRejectDOMRequest(POST_ERROR_EVENT_UNKNOWN, aRv);
    }

    return ds->AddOrAppendNamed(aBlob, storagePath, aCreate, aRv);
  }

  nsRefPtr<DOMRequest> domRequest;
  uint32_t id = CreateDOMRequest(getter_AddRefs(domRequest), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<DeviceStorageFile> dsf;
  if (aPath.IsEmpty()) {
    dsf = new DeviceStorageFile(mStorageType, mStorageName);
  } else {
    dsf = new DeviceStorageFile(mStorageType, mStorageName, aPath);
    if (!dsf->IsSafePath()) {
      aRv = mManager->Reject(id, POST_ERROR_EVENT_PERMISSION_DENIED);
      return domRequest.forget();
    }
  }

  nsRefPtr<DeviceStorageRequest> request;
  if (aCreate) {
    request = new DeviceStorageCreateRequest();
  } else {
    request = new DeviceStorageAppendRequest();
  }
  request->Initialize(mManager, dsf, id, aBlob->Impl());
  aRv = CheckPermission(request);
  return domRequest.forget();
}

already_AddRefed<DOMRequest>
nsDOMDeviceStorage::GetInternal(const nsAString& aPath, bool aEditable,
                                ErrorResult& aRv)
{
  MOZ_ASSERT(IsOwningThread());

  if (IsFullPath(aPath)) {
    nsString storagePath;
    nsRefPtr<nsDOMDeviceStorage> ds = GetStorage(aPath, storagePath);
    if (!ds) {
      return CreateAndRejectDOMRequest(POST_ERROR_EVENT_UNKNOWN, aRv);
    }
    return ds->GetInternal(storagePath, aEditable, aRv);
  }

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType,
                                                          mStorageName,
                                                          aPath);
  dsf->SetEditable(aEditable);
  if (!dsf->IsSafePath()) {
    return CreateAndRejectDOMRequest(POST_ERROR_EVENT_PERMISSION_DENIED, aRv);
  }

  nsRefPtr<DOMRequest> domRequest;
  uint32_t id = CreateDOMRequest(getter_AddRefs(domRequest), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<DeviceStorageRequest> request = new DeviceStorageOpenRequest();
  request->Initialize(mManager, dsf, id);

  aRv = CheckPermission(request);
  return domRequest.forget();
}

already_AddRefed<DOMRequest>
nsDOMDeviceStorage::Delete(const nsAString& aPath, ErrorResult& aRv)
{
  MOZ_ASSERT(IsOwningThread());

  if (IsFullPath(aPath)) {
    nsString storagePath;
    nsRefPtr<nsDOMDeviceStorage> ds = GetStorage(aPath, storagePath);
    if (!ds) {
      return CreateAndRejectDOMRequest(POST_ERROR_EVENT_UNKNOWN, aRv);
    }
    return ds->Delete(storagePath, aRv);
  }

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType,
                                                          mStorageName,
                                                          aPath);
  if (!dsf->IsSafePath()) {
    return CreateAndRejectDOMRequest(POST_ERROR_EVENT_PERMISSION_DENIED, aRv);
  }

  nsRefPtr<DOMRequest> domRequest;
  uint32_t id = CreateDOMRequest(getter_AddRefs(domRequest), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<DeviceStorageRequest> request = new DeviceStorageDeleteRequest();
  request->Initialize(mManager, dsf, id);

  aRv = CheckPermission(request);
  return domRequest.forget();
}

already_AddRefed<DOMRequest>
nsDOMDeviceStorage::FreeSpace(ErrorResult& aRv)
{
  MOZ_ASSERT(IsOwningThread());

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType,
                                                          mStorageName);

  nsRefPtr<DOMRequest> domRequest;
  uint32_t id = CreateDOMRequest(getter_AddRefs(domRequest), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<DeviceStorageRequest> request = new DeviceStorageFreeSpaceRequest();
  request->Initialize(mManager, dsf, id);

  aRv = CheckPermission(request);
  return domRequest.forget();
}

already_AddRefed<DOMRequest>
nsDOMDeviceStorage::UsedSpace(ErrorResult& aRv)
{
  MOZ_ASSERT(IsOwningThread());

  DebugOnly<DeviceStorageUsedSpaceCache*> usedSpaceCache
    = DeviceStorageUsedSpaceCache::CreateOrGet();
  MOZ_ASSERT(usedSpaceCache);

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType,
                                                          mStorageName);

  nsRefPtr<DOMRequest> domRequest;
  uint32_t id = CreateDOMRequest(getter_AddRefs(domRequest), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<DeviceStorageRequest> request = new DeviceStorageUsedSpaceRequest();
  request->Initialize(mManager, dsf, id);

  aRv = CheckPermission(request);
  return domRequest.forget();
}

already_AddRefed<DOMRequest>
nsDOMDeviceStorage::Available(ErrorResult& aRv)
{
  MOZ_ASSERT(IsOwningThread());

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType,
                                                          mStorageName);

  nsRefPtr<DOMRequest> domRequest;
  uint32_t id = CreateDOMRequest(getter_AddRefs(domRequest), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<DeviceStorageRequest> request = new DeviceStorageAvailableRequest();
  request->Initialize(mManager, dsf, id);

  aRv = CheckPermission(request);
  return domRequest.forget();
}

already_AddRefed<DOMRequest>
nsDOMDeviceStorage::StorageStatus(ErrorResult& aRv)
{
  MOZ_ASSERT(IsOwningThread());

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType,
                                                          mStorageName);

  nsRefPtr<DOMRequest> domRequest;
  uint32_t id = CreateDOMRequest(getter_AddRefs(domRequest), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<DeviceStorageRequest> request = new DeviceStorageStatusRequest();
  request->Initialize(mManager, dsf, id);

  aRv = CheckPermission(request);
  return domRequest.forget();
}

already_AddRefed<DOMRequest>
nsDOMDeviceStorage::Format(ErrorResult& aRv)
{
  MOZ_ASSERT(IsOwningThread());

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType,
                                                          mStorageName);

  nsRefPtr<DOMRequest> domRequest;
  uint32_t id = CreateDOMRequest(getter_AddRefs(domRequest), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<DeviceStorageRequest> request = new DeviceStorageFormatRequest();
  request->Initialize(mManager, dsf, id);

  aRv = CheckPermission(request);
  return domRequest.forget();
}

already_AddRefed<DOMRequest>
nsDOMDeviceStorage::Mount(ErrorResult& aRv)
{
  MOZ_ASSERT(IsOwningThread());

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType,
                                                          mStorageName);

  nsRefPtr<DOMRequest> domRequest;
  uint32_t id = CreateDOMRequest(getter_AddRefs(domRequest), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<DeviceStorageRequest> request = new DeviceStorageMountRequest();
  request->Initialize(mManager, dsf, id);

  aRv = CheckPermission(request);
  return domRequest.forget();
}

already_AddRefed<DOMRequest>
nsDOMDeviceStorage::Unmount(ErrorResult& aRv)
{
  MOZ_ASSERT(IsOwningThread());

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType,
                                                          mStorageName);

  nsRefPtr<DOMRequest> domRequest;
  uint32_t id = CreateDOMRequest(getter_AddRefs(domRequest), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }


  nsRefPtr<DeviceStorageRequest> request = new DeviceStorageUnmountRequest();
  request->Initialize(mManager, dsf, id);

  aRv = CheckPermission(request);
  return domRequest.forget();
}

already_AddRefed<DOMRequest>
nsDOMDeviceStorage::CreateFileDescriptor(const nsAString& aPath,
                                         DeviceStorageFileDescriptor* aDSFileDescriptor,
                                         ErrorResult& aRv)
{
  MOZ_ASSERT(IsOwningThread());
  MOZ_ASSERT(aDSFileDescriptor);

  DeviceStorageTypeChecker* typeChecker
    = DeviceStorageTypeChecker::CreateOrGet();
  if (!typeChecker) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  if (IsFullPath(aPath)) {
    nsString storagePath;
    nsRefPtr<nsDOMDeviceStorage> ds = GetStorage(aPath, storagePath);
    if (!ds) {
      return CreateAndRejectDOMRequest(POST_ERROR_EVENT_UNKNOWN, aRv);
    }
    return ds->CreateFileDescriptor(storagePath, aDSFileDescriptor, aRv);
  }

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType,
                                                          mStorageName,
                                                          aPath);
  if (!dsf->IsSafePath()) {
    return CreateAndRejectDOMRequest(POST_ERROR_EVENT_PERMISSION_DENIED, aRv);
  }

  if (!typeChecker->Check(mStorageType, dsf->mFile)) {
    return CreateAndRejectDOMRequest(POST_ERROR_EVENT_ILLEGAL_TYPE, aRv);
  }

  nsRefPtr<DOMRequest> domRequest;
  uint32_t id = CreateDOMRequest(getter_AddRefs(domRequest), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<DeviceStorageRequest> request = new DeviceStorageCreateFdRequest();
  request->Initialize(mManager, dsf, id, aDSFileDescriptor);

  aRv = CheckPermission(request);
  return domRequest.forget();
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

bool
nsDOMDeviceStorage::IsRemovable()
{
  return mIsRemovable;
}

bool
nsDOMDeviceStorage::LowDiskSpace()
{
  return DeviceStorageStatics::LowDiskSpace();
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

void
nsDOMDeviceStorage::GetStorageName(nsAString& aStorageName)
{
  aStorageName = mStorageName;
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
  MOZ_ASSERT(IsOwningThread());

  PRTime since = 0;
  if (aOptions.mSince.WasPassed() && !aOptions.mSince.Value().IsUndefined()) {
    since = PRTime(aOptions.mSince.Value().TimeStamp().toDouble());
  }

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType,
                                                          mStorageName,
                                                          aPath,
                                                          EmptyString());
  dsf->SetEditable(aEditable);

  nsRefPtr<DeviceStorageCursorRequest> request = new DeviceStorageCursorRequest();
  nsRefPtr<nsDOMDeviceStorageCursor> cursor;
  uint32_t id = CreateDOMCursor(request, getter_AddRefs(cursor), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (!dsf->IsSafePath()) {
    aRv = mManager->Reject(id, POST_ERROR_EVENT_PERMISSION_DENIED);
  } else {
    request->Initialize(mManager, dsf, id, since);
    aRv = CheckPermission(request);
  }

  return cursor.forget();
}

void
nsDOMDeviceStorage::OnWritableNameChanged()
{
  nsAdoptingString DefaultLocation;
  GetDefaultStorageName(mStorageType, DefaultLocation);

  DeviceStorageChangeEventInit init;
  init.mBubbles = true;
  init.mCancelable = false;
  init.mPath = DefaultLocation;

  if (mIsDefaultLocation) {
    init.mReason.AssignLiteral("default-location-changed");
  } else {
    init.mReason.AssignLiteral("became-default-location");
  }

  nsRefPtr<DeviceStorageChangeEvent> event =
    DeviceStorageChangeEvent::Constructor(this,
                                          NS_LITERAL_STRING(STORAGE_CHANGE_EVENT),
                                          init);
  event->SetTrusted(true);

  bool ignore;
  DispatchEvent(event, &ignore);
  mIsDefaultLocation = Default();
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
    DeviceStorageChangeEvent::Constructor(this,
                                          NS_LITERAL_STRING(STORAGE_CHANGE_EVENT),
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

void
nsDOMDeviceStorage::OnFileWatcherUpdate(const nsCString& aData, DeviceStorageFile* aFile)
{
  MOZ_ASSERT(IsOwningThread());
  Notify(aData.get(), aFile);
}

void
nsDOMDeviceStorage::OnDiskSpaceWatcher(bool aLowDiskSpace)
{
  MOZ_ASSERT(IsOwningThread());
  nsRefPtr<DeviceStorageFile> file =
    new DeviceStorageFile(mStorageType, mStorageName);
  if (aLowDiskSpace) {
    Notify("low-disk-space", file);
  } else {
    Notify("available-disk-space", file);
  }
}

#ifdef MOZ_WIDGET_GONK
void
nsDOMDeviceStorage::OnVolumeStateChanged(nsIVolume* aVolume) {
  MOZ_ASSERT(IsOwningThread());

  // We invalidate the used space cache for the volume that actually changed
  // state.
  nsString volName;
  aVolume->GetName(volName);

  DeviceStorageUsedSpaceCache* usedSpaceCache
    = DeviceStorageUsedSpaceCache::CreateOrGet();
  MOZ_ASSERT(usedSpaceCache);
  usedSpaceCache->Invalidate(volName);

  if (!volName.Equals(mStorageName)) {
    // Not our volume - we can ignore.
    return;
  }

  nsRefPtr<DeviceStorageFile> dsf(new DeviceStorageFile(mStorageType, mStorageName));
  nsString status, storageStatus;

  // Get Status (one of "available, unavailable, shared")
  dsf->GetStatus(status);
  DispatchStatusChangeEvent(status);

  // Get real volume status (defined in dom/system/gonk/nsIVolume.idl)
  dsf->GetStorageStatus(storageStatus);
  DispatchStorageStatusChangeEvent(storageStatus);
}
#endif

nsresult
nsDOMDeviceStorage::Notify(const char* aReason, DeviceStorageFile* aFile)
{
  if (!mManager) {
    return NS_OK;
  }

  if (mManager->CheckPermission(DEVICE_STORAGE_ACCESS_READ) !=
      nsIPermissionManager::ALLOW_ACTION) {
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
    DeviceStorageChangeEvent::Constructor(this,
                                          NS_LITERAL_STRING(STORAGE_CHANGE_EVENT),
                                          init);
  event->SetTrusted(true);

  bool ignore;
  DispatchEvent(event, &ignore);
  return NS_OK;
}

void
nsDOMDeviceStorage::EventListenerWasAdded(const nsAString& aType,
                                          ErrorResult& aRv,
                                          JSCompartment* aCompartment)
{
  MOZ_ASSERT(IsOwningThread());

  if (!mManager) {
    return;
  }

  if (mManager->CheckPermission(DEVICE_STORAGE_ACCESS_READ) !=
      nsIPermissionManager::PROMPT_ACTION) {
    return;
  }

  if (!aType.EqualsLiteral(STORAGE_CHANGE_EVENT)) {
    return;
  }

  nsRefPtr<DOMRequest> domRequest;
  uint32_t id = CreateDOMRequest(getter_AddRefs(domRequest), aRv);
  if (aRv.Failed()) {
    return;
  }

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType,
                                                          mStorageName);
  nsRefPtr<DeviceStorageRequest> request = new DeviceStorageWatchRequest();
  request->Initialize(mManager, dsf, id);
  aRv = CheckPermission(request);
}

Atomic<uint32_t> DeviceStorageRequestManager::sLastRequestId(0);

DeviceStorageRequestManager::DeviceStorageRequestManager()
  : mOwningThread(NS_GetCurrentThread())
  , mMutex("DeviceStorageRequestManager::mMutex")
  , mShutdown(false)
{
  DS_LOG_INFO("%p", this);
  for (size_t i = 0; i < MOZ_ARRAY_LENGTH(mPermissionCache); ++i) {
    mPermissionCache[i] = nsIPermissionManager::PROMPT_ACTION;
  }
}

DeviceStorageRequestManager::~DeviceStorageRequestManager()
{
  DS_LOG_INFO("%p pending %zu", this, mPending.Length());

  if (!mPending.IsEmpty()) {
    MOZ_ASSERT_UNREACHABLE("Should not destroy, still has pending requests");
    ListIndex i = mPending.Length();
    while (i > 0) {
      --i;
      DS_LOG_ERROR("terminate %u", mPending[i].mId);
      NS_ProxyRelease(mOwningThread,
        NS_ISUPPORTS_CAST(EventTarget*, mPending[i].mRequest.forget().take()));
    }
  }
}

void
DeviceStorageRequestManager::StorePermission(size_t aAccess, bool aAllow)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aAccess < MOZ_ARRAY_LENGTH(mPermissionCache));

  MutexAutoLock lock(mMutex);
  mPermissionCache[aAccess] = aAllow ? nsIPermissionManager::ALLOW_ACTION
                                     : nsIPermissionManager::DENY_ACTION;
  DS_LOG_INFO("access %zu cache %u", aAccess, mPermissionCache[aAccess]);
}

uint32_t
DeviceStorageRequestManager::CheckPermission(size_t aAccess)
{
  MOZ_ASSERT(IsOwningThread() || NS_IsMainThread());
  MOZ_ASSERT(aAccess < MOZ_ARRAY_LENGTH(mPermissionCache));

  MutexAutoLock lock(mMutex);
  DS_LOG_INFO("access %zu cache %u", aAccess, mPermissionCache[aAccess]);
  return mPermissionCache[aAccess];
}

bool
DeviceStorageRequestManager::IsOwningThread()
{
  bool owner = false;
  mOwningThread->IsOnCurrentThread(&owner);
  return owner;
}

nsresult
DeviceStorageRequestManager::DispatchToOwningThread(nsIRunnable* aRunnable)
{
  return mOwningThread->Dispatch(aRunnable, NS_DISPATCH_NORMAL);
}

nsresult
DeviceStorageRequestManager::DispatchOrAbandon(uint32_t aId,
                                               nsIRunnable* aRunnable)
{
  MutexAutoLock lock(mMutex);
  if (mShutdown) {
    /* Dispatching in this situation may result in the runnable being
       silently discarded but not freed. The runnables themselves are
       safe to be freed off the owner thread but the dispatch method
       does not know that. */
    DS_LOG_DEBUG("shutdown %u", aId);
    return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
  }

  nsresult rv = DispatchToOwningThread(aRunnable);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    DS_LOG_ERROR("abandon %u", aId);
  }
  return rv;
}

uint32_t
DeviceStorageRequestManager::Create(nsDOMDeviceStorage* aDeviceStorage,
                                    DeviceStorageCursorRequest* aRequest,
                                    nsDOMDeviceStorageCursor** aCursor)
{
  MOZ_ASSERT(aDeviceStorage);
  MOZ_ASSERT(aRequest);
  MOZ_ASSERT(aCursor);

  nsRefPtr<nsDOMDeviceStorageCursor> request
    = new nsDOMDeviceStorageCursor(aDeviceStorage->GetOwnerGlobal(), aRequest);
  uint32_t id = CreateInternal(request, true);
  DS_LOG_INFO("%u", id);
  request.forget(aCursor);
  return id;
}

uint32_t
DeviceStorageRequestManager::Create(nsDOMDeviceStorage* aDeviceStorage,
                                    DOMRequest** aRequest)
{
  MOZ_ASSERT(aDeviceStorage);
  MOZ_ASSERT(aRequest);

  nsRefPtr<DOMRequest> request
    = new DOMRequest(aDeviceStorage->GetOwnerGlobal());
  uint32_t id = CreateInternal(request, false);
  DS_LOG_INFO("%u", id);
  request.forget(aRequest);
  return id;
}

uint32_t
DeviceStorageRequestManager::CreateInternal(DOMRequest* aRequest, bool aCursor)
{
  MOZ_ASSERT(IsOwningThread());
  MOZ_ASSERT(!mShutdown);

  uint32_t id;
  do {
    id = ++sLastRequestId;
  } while (id == INVALID_ID || Find(id) != mPending.Length());

  ListEntry* entry = mPending.AppendElement();
  entry->mId = id;
  entry->mRequest = aRequest;
  entry->mCursor = aCursor;
  return entry->mId;
}

nsresult
DeviceStorageRequestManager::Resolve(uint32_t aId, bool aForceDispatch)
{
  if (aForceDispatch || !IsOwningThread()) {
    DS_LOG_DEBUG("recv %u", aId);
    nsRefPtr<DeviceStorageRequestManager> self = this;
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([self, aId] () -> void
    {
      self->Resolve(aId, false);
    });
    return DispatchOrAbandon(aId, r);
  }

  DS_LOG_INFO("posted %u", aId);

  if (NS_WARN_IF(aId == INVALID_ID)) {
    DS_LOG_ERROR("invalid");
    MOZ_ASSERT_UNREACHABLE("resolve invalid request");
    return NS_OK;
  }

  ListIndex i = Find(aId);
  if (NS_WARN_IF(i == mPending.Length())) {
    return NS_OK;
  }

  return ResolveInternal(i, JS::UndefinedHandleValue);
}

nsresult
DeviceStorageRequestManager::Resolve(uint32_t aId, const nsString& aResult,
                                     bool aForceDispatch)
{
  if (aForceDispatch || !IsOwningThread()) {
    DS_LOG_DEBUG("recv %u", aId);
    nsRefPtr<DeviceStorageRequestManager> self = this;
    nsString result = aResult;
    nsCOMPtr<nsIRunnable> r
      = NS_NewRunnableFunction([self, aId, result] () -> void
    {
      self->Resolve(aId, result, false);
    });
    return DispatchOrAbandon(aId, r);
  }

  DS_LOG_INFO("posted %u w/ %s", aId,
              NS_LossyConvertUTF16toASCII(aResult).get());

  if (NS_WARN_IF(aId == INVALID_ID)) {
    DS_LOG_WARN("invalid");
    MOZ_ASSERT_UNREACHABLE("resolve invalid request");
    return NS_OK;
  }

  ListIndex i = Find(aId);
  if (NS_WARN_IF(i == mPending.Length())) {
    return NS_OK;
  }

  nsIGlobalObject* global = mPending[i].mRequest->GetOwnerGlobal();

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(global))) {
    return RejectInternal(i, NS_LITERAL_STRING(POST_ERROR_EVENT_UNKNOWN));
  }

  JS::RootedValue rvalue(jsapi.cx());
  JS::MutableHandleValue mvalue(&rvalue);
  if (NS_WARN_IF(!xpc::StringToJsval(jsapi.cx(), aResult, mvalue))) {
    return RejectInternal(i, NS_LITERAL_STRING(POST_ERROR_EVENT_UNKNOWN));
  }

  return ResolveInternal(i, rvalue);
}

nsresult
DeviceStorageRequestManager::Resolve(uint32_t aId, uint64_t aValue,
                                     bool aForceDispatch)
{
  if (aForceDispatch || !IsOwningThread()) {
    DS_LOG_DEBUG("recv %u w/ %" PRIu64, aId, aValue);
    nsRefPtr<DeviceStorageRequestManager> self = this;
    nsCOMPtr<nsIRunnable> r
      = NS_NewRunnableFunction([self, aId, aValue] () -> void
    {
      self->Resolve(aId, aValue, false);
    });
    return DispatchOrAbandon(aId, r);
  }

  DS_LOG_INFO("posted %u w/ %" PRIu64, aId, aValue);

  if (NS_WARN_IF(aId == INVALID_ID)) {
    DS_LOG_ERROR("invalid");
    MOZ_ASSERT_UNREACHABLE("resolve invalid request");
    return NS_OK;
  }

  ListIndex i = Find(aId);
  if (NS_WARN_IF(i == mPending.Length())) {
    return NS_OK;
  }

  nsIGlobalObject* global = mPending[i].mRequest->GetOwnerGlobal();

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(global))) {
    return RejectInternal(i, NS_LITERAL_STRING(POST_ERROR_EVENT_UNKNOWN));
  }

  JS::RootedValue value(jsapi.cx(), JS_NumberValue((double)aValue));
  return ResolveInternal(i, value);
}

nsresult
DeviceStorageRequestManager::Resolve(uint32_t aId, DeviceStorageFile* aFile,
                                     bool aForceDispatch)
{
  MOZ_ASSERT(aFile);
  DS_LOG_DEBUG("recv %u w/ %p", aId, aFile);

  nsString fullPath;
  aFile->GetFullPath(fullPath);

  /* This check is useful to know if somewhere the DeviceStorageFile
     has not been properly set. Mimetype is not checked because it can be
     empty. */
  MOZ_ASSERT(aFile->mLength != UINT64_MAX);
  MOZ_ASSERT(aFile->mLastModifiedDate != UINT64_MAX);

  nsRefPtr<BlobImpl> blobImpl = new BlobImplFile(fullPath, aFile->mMimeType,
                                                 aFile->mLength, aFile->mFile,
                                                 aFile->mLastModifiedDate);

  /* File should start out as mutable by default but we should turn
     that off if it wasn't requested. */
  bool editable;
  nsresult rv = blobImpl->GetMutable(&editable);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    DS_LOG_WARN("%u cannot query mutable", aId);
    return Reject(aId, POST_ERROR_EVENT_UNKNOWN);
  }

  if (editable != aFile->mEditable) {
    rv = blobImpl->SetMutable(aFile->mEditable);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      DS_LOG_WARN("%u cannot set mutable %d", aId, aFile->mEditable);
      return Reject(aId, POST_ERROR_EVENT_UNKNOWN);
    }
  }

  return Resolve(aId, blobImpl, aForceDispatch);
}

nsresult
DeviceStorageRequestManager::Resolve(uint32_t aId, BlobImpl* aBlobImpl,
                                     bool aForceDispatch)
{
  if (aForceDispatch || !IsOwningThread()) {
    DS_LOG_DEBUG("recv %u w/ %p", aId, aBlobImpl);
    nsRefPtr<DeviceStorageRequestManager> self = this;
    nsRefPtr<BlobImpl> blobImpl = aBlobImpl;
    nsCOMPtr<nsIRunnable> r
      = NS_NewRunnableFunction([self, aId, blobImpl] () -> void
    {
      self->Resolve(aId, blobImpl, false);
    });
    return DispatchOrAbandon(aId, r);
  }

  DS_LOG_INFO("posted %u w/ %p", aId, aBlobImpl);

  if (NS_WARN_IF(aId == INVALID_ID)) {
    DS_LOG_ERROR("invalid");
    MOZ_ASSERT_UNREACHABLE("resolve invalid request");
    return NS_OK;
  }

  ListIndex i = Find(aId);
  if (NS_WARN_IF(i == mPending.Length())) {
    return NS_OK;
  }

  if (!aBlobImpl) {
    return ResolveInternal(i, JS::NullHandleValue);
  }

  nsIGlobalObject* global = mPending[i].mRequest->GetOwnerGlobal();

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(global))) {
    return RejectInternal(i, NS_LITERAL_STRING(POST_ERROR_EVENT_UNKNOWN));
  }

  nsRefPtr<Blob> blob = Blob::Create(global, aBlobImpl);
  JS::Rooted<JSObject*> obj(jsapi.cx(),
                            blob->WrapObject(jsapi.cx(), nullptr));
  MOZ_ASSERT(obj);
  JS::RootedValue value(jsapi.cx(), JS::ObjectValue(*obj));
  return ResolveInternal(i, value);
}

nsresult
DeviceStorageRequestManager::ResolveInternal(ListIndex aIndex,
                                             JS::HandleValue aResult)
{
  MOZ_ASSERT(IsOwningThread());
  MOZ_ASSERT(!mShutdown);

  /* Note that we must seize the DOMRequest reference and destroy the entry
     before calling FireError because it may go straight to the JS code
     which in term may call back into our code (as observed in Shutdown).
     The safest thing to do is to ensure the very last thing we do is the
     DOM call so that there is no inconsistent state. */
  nsRefPtr<DOMRequest> request;
  bool isCursor = mPending[aIndex].mCursor;
  if (!isCursor || aResult.isUndefined()) {
    request = mPending[aIndex].mRequest.forget();
    mPending.RemoveElementAt(aIndex);
  } else {
    request = mPending[aIndex].mRequest;
  }

  if (isCursor) {
    auto cursor = static_cast<nsDOMDeviceStorageCursor*>(request.get());

    /* Must call it with the right pointer type since the base class does
       not define FireDone and FireSuccess as virtual. */
    if (aResult.isUndefined()) {
      DS_LOG_INFO("cursor complete");
      cursor->FireDone();
    } else {
      DS_LOG_INFO("cursor continue");
      cursor->FireSuccess(aResult);
    }
  } else {
    DS_LOG_INFO("request complete");
    request->FireSuccess(aResult);
  }
  return NS_OK;
}

nsresult
DeviceStorageRequestManager::RejectInternal(DeviceStorageRequestManager::ListIndex aIndex,
                                            const nsString& aReason)
{
  MOZ_ASSERT(IsOwningThread());
  MOZ_ASSERT(!mShutdown);

  /* Note that we must seize the DOMRequest reference and destroy the entry
     before calling FireError because it may go straight to the JS code
     which in term may call back into our code (as observed in Shutdown).
     The safest thing to do is to ensure the very last thing we do is the
     DOM call so that there is no inconsistent state. */
  nsRefPtr<DOMRequest> request = mPending[aIndex].mRequest.forget();
  bool isCursor = mPending[aIndex].mCursor;
  mPending.RemoveElementAt(aIndex);

  if (isCursor) {
    /* Must call it with the right pointer type since the base class does
       not define FireError as virtual. */
    auto cursor = static_cast<nsDOMDeviceStorageCursor*>(request.get());
    cursor->FireError(aReason);
  } else {
    request->FireError(aReason);
  }
  return NS_OK;
}

nsresult
DeviceStorageRequestManager::Reject(uint32_t aId, const nsString& aReason)
{
  DS_LOG_DEBUG("recv %u", aId);

  if (NS_WARN_IF(aId == INVALID_ID)) {
    DS_LOG_ERROR("invalid");
    MOZ_ASSERT_UNREACHABLE("reject invalid request");
    return NS_OK;
  }

  nsRefPtr<DeviceStorageRequestManager> self = this;
  nsString reason = aReason;
  nsCOMPtr<nsIRunnable> r
    = NS_NewRunnableFunction([self, aId, reason] () -> void
  {
    DS_LOG_INFO("posted %u w/ %s", aId,
                NS_LossyConvertUTF16toASCII(reason).get());

    ListIndex i = self->Find(aId);
    if (NS_WARN_IF(i == self->mPending.Length())) {
      return;
    }

    self->RejectInternal(i, reason);
  });
  return DispatchOrAbandon(aId, r);
}

nsresult
DeviceStorageRequestManager::Reject(uint32_t aId, const char* aReason)
{
  nsString reason;
  reason.AssignASCII(aReason);
  return Reject(aId, reason);
}

void
DeviceStorageRequestManager::Shutdown()
{
  MOZ_ASSERT(IsOwningThread());

  MutexAutoLock lock(mMutex);
  mShutdown = true;
  ListIndex i = mPending.Length();
  DS_LOG_INFO("pending %zu", i);
  while (i > 0) {
    --i;
    DS_LOG_INFO("terminate %u (%u)", mPending[i].mId, mPending[i].mCursor);
  }
  mPending.Clear();
}

DeviceStorageRequestManager::ListIndex
DeviceStorageRequestManager::Find(uint32_t aId)
{
  MOZ_ASSERT(IsOwningThread());
  ListIndex i = mPending.Length();
  while (i > 0) {
    --i;
    if (mPending[i].mId == aId) {
      return i;
    }
  }
  DS_LOG_DEBUG("no id %u", aId);
  return mPending.Length();
}
