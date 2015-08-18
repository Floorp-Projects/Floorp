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
DeviceStorageTypeChecker::Check(const nsAString& aType, Blob* aBlob)
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
    return JS::NullValue();
  }

  if (aFile->mEditable) {
    // TODO - needs janv's file handle support.
    return JS::NullValue();
  }

  nsString fullPath;
  aFile->GetFullPath(fullPath);

  // This check is useful to know if somewhere the DeviceStorageFile
  // has not been properly set. Mimetype is not checked because it can be
  // empty.
  MOZ_ASSERT(aFile->mLength != UINT64_MAX);
  MOZ_ASSERT(aFile->mLastModifiedDate != UINT64_MAX);

  nsCOMPtr<nsIDOMBlob> blob = Blob::Create(aWindow,
    new BlobImplFile(fullPath, aFile->mMimeType,
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

class DeviceStorageCursorRequest final
  : public nsIContentPermissionRequest
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(DeviceStorageCursorRequest,
                                           nsIContentPermissionRequest)

  NS_FORWARD_NSICONTENTPERMISSIONREQUEST(mCursor->);

  explicit DeviceStorageCursorRequest(nsDOMDeviceStorageCursor* aCursor)
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

  while (cursor->mIndex < cursor->mFiles.Length()) {
    nsRefPtr<DeviceStorageFile> file = cursor->mFiles[cursor->mIndex].forget();
    ++cursor->mIndex;
    if (file) {
      file->CalculateMimeType();
      return file.forget();
    }
  }

  return nullptr;
}

ContinueCursorEvent::~ContinueCursorEvent() {}

void
ContinueCursorEvent::Continue()
{
  if (XRE_IsParentProcess()) {
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
  , mIndex(0)
  , mFile(aFile)
  , mPrincipal(aPrincipal)
  , mRequester(new nsContentPermissionRequester(GetOwner()))
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
nsDOMDeviceStorageCursor::GetPrincipal(nsIPrincipal** aRequestingPrincipal)
{
  NS_IF_ADDREF(*aRequestingPrincipal = mPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceStorageCursor::GetWindow(nsIDOMWindow** aRequestingWindow)
{
  NS_IF_ADDREF(*aRequestingWindow = GetOwner());
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceStorageCursor::GetElement(nsIDOMElement** aRequestingElement)
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

  if (!XRE_IsParentProcess()) {
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

NS_IMETHODIMP
nsDOMDeviceStorageCursor::GetRequester(nsIContentPermissionRequester** aRequester)
{
  NS_ENSURE_ARG_POINTER(aRequester);

  nsCOMPtr<nsIContentPermissionRequester> requester = mRequester;
  requester.forget(aRequester);
  return NS_OK;
}

void
nsDOMDeviceStorageCursor::Continue(ErrorResult& aRv)
{
  if (!mOkToCallContinue) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  if (!mResult.isUndefined()) {
    // We call onsuccess multiple times. Clear the last
    // result.
    mResult.setUndefined();
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
    JS::Rooted<JS::Value> result(cx, JS::NullValue());

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
    MOZ_ASSERT(mDSFileDescriptor->mDSFile);
    MOZ_ASSERT(mDSFileDescriptor->mDSFile->mFile);
    MOZ_ASSERT(mRequest);
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    DeviceStorageFile* dsFile = mDSFileDescriptor->mDSFile;

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
  WriteFileEvent(BlobImpl* aBlobImpl,
                 DeviceStorageFile *aFile,
                 already_AddRefed<DOMRequest> aRequest,
                 int32_t aRequestType)
    : mBlobImpl(aBlobImpl)
    , mFile(aFile)
    , mRequest(aRequest)
    , mRequestType(aRequestType)
  {
    MOZ_ASSERT(mFile);
    MOZ_ASSERT(mFile->mFile);
    MOZ_ASSERT(mRequest);
    MOZ_ASSERT(mRequestType);
  }

  ~WriteFileEvent() {}

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    ErrorResult rv;
    nsCOMPtr<nsIInputStream> stream;
    mBlobImpl->GetInternalStream(getter_AddRefs(stream), rv);
    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
      nsCOMPtr<nsIRunnable> event =
        new PostErrorEvent(mRequest.forget(), POST_ERROR_EVENT_UNKNOWN);
      return NS_DispatchToMainThread(event);
    }

    bool check = false;
    mFile->mFile->Exists(&check);

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
      if (NS_WARN_IF(rv.Failed())) {
        rv.SuppressException();
        mFile->mFile->Remove(false);
      }
    } else {
      nsCOMPtr<nsIRunnable> event =
        new PostErrorEvent(mRequest.forget(), POST_ERROR_EVENT_UNKNOWN);
      return NS_DispatchToMainThread(event);
    }

    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();

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
  nsRefPtr<BlobImpl> mBlobImpl;
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
      mFile->GetStorageFreeSpace(&freeSpace);
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

class DeviceStorageRequest final
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
    , mRequester(new nsContentPermissionRequester(mWindow))
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
                       Blob* aBlob = nullptr)
    : mRequestType(aRequestType)
    , mWindow(aWindow)
    , mPrincipal(aPrincipal)
    , mFile(aFile)
    , mRequest(aRequest)
    , mBlob(aBlob)
    , mRequester(new nsContentPermissionRequester(mWindow))
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
    , mRequester(new nsContentPermissionRequester(mWindow))
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

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (DeviceStorageStatics::IsPromptTesting()) {
      Allow(JS::UndefinedHandleValue);
      return NS_OK;
    }

    return nsContentPermissionUtils::AskPermission(this, mWindow);
  }

  NS_IMETHODIMP GetTypes(nsIArray** aTypes) override
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

  NS_IMETHOD Cancel() override
  {
    nsCOMPtr<nsIRunnable> event
      = new PostErrorEvent(mRequest.forget(),
                           POST_ERROR_EVENT_PERMISSION_DENIED);
    return NS_DispatchToMainThread(event);
  }

  NS_IMETHOD Allow(JS::HandleValue aChoices) override
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

        if (!XRE_IsParentProcess()) {

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

        if (!XRE_IsParentProcess()) {
          BlobChild* actor
            = ContentChild::GetSingleton()->GetOrCreateActorForBlob(
              static_cast<Blob*>(mBlob.get()));
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

        File* blob = static_cast<File*>(mBlob.get());
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

        if (!XRE_IsParentProcess()) {
          BlobChild* actor
            = ContentChild::GetSingleton()->GetOrCreateActorForBlob(
              static_cast<Blob*>(mBlob.get()));
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

        File* blob = static_cast<File*>(mBlob.get());
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

        if (!XRE_IsParentProcess()) {
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

        if (!XRE_IsParentProcess()) {
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
        if (!XRE_IsParentProcess()) {
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
        if (!XRE_IsParentProcess()) {
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
        if (!XRE_IsParentProcess()) {
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
        if (!XRE_IsParentProcess()) {
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
        if (!XRE_IsParentProcess()) {
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
        if (!XRE_IsParentProcess()) {
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
        if (!XRE_IsParentProcess()) {
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

  NS_IMETHOD GetRequester(nsIContentPermissionRequester** aRequester) override
  {
    NS_ENSURE_ARG_POINTER(aRequester);

    nsCOMPtr<nsIContentPermissionRequester> requester = mRequester;
    requester.forget(aRequester);
    return NS_OK;
  }

private:
  ~DeviceStorageRequest() {}

  int32_t mRequestType;
  nsCOMPtr<nsPIDOMWindow> mWindow;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsRefPtr<DeviceStorageFile> mFile;

  nsRefPtr<DOMRequest> mRequest;
  nsRefPtr<Blob> mBlob;
  nsRefPtr<nsDOMDeviceStorage> mDeviceStorage;
  nsRefPtr<DeviceStorageFileDescriptor> mDSFileDescriptor;
  nsCOMPtr<nsIContentPermissionRequester> mRequester;
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
  , mIsWatchingFile(false)
  , mAllowedToWatchFile(false)
{
  MOZ_ASSERT(NS_IsMainThread());
  sInstanceCount++;
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

  SetRootDirectoryForType(aType, aVolName);
  if (!mRootDirectory) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  DeviceStorageStatics::AddListener(this);
  if (!mStorageName.IsEmpty()) {
    mIsDefaultLocation = Default();

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
      bool isRemovable;
      rv = vol->GetIsHotSwappable(&isRemovable);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      mIsRemovable = isRemovable;
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
  MOZ_ASSERT(NS_IsMainThread());
  sInstanceCount--;
  DeviceStorageStatics::RemoveListener(this);
}

void
nsDOMDeviceStorage::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());

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

  nsCOMPtr<nsPIDOMWindow> window = GetOwner();
  return aWin && aWin == window &&
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
  MOZ_ASSERT(NS_IsMainThread());

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

already_AddRefed<DOMRequest>
nsDOMDeviceStorage::AddNamed(Blob* aBlob, const nsAString& aPath,
                             ErrorResult& aRv)
{
  return AddOrAppendNamed(aBlob, aPath,
                          DEVICE_STORAGE_REQUEST_CREATE, aRv);
}

already_AddRefed<DOMRequest>
nsDOMDeviceStorage::AppendNamed(Blob* aBlob, const nsAString& aPath,
                                ErrorResult& aRv)
{
  return AddOrAppendNamed(aBlob, aPath,
                          DEVICE_STORAGE_REQUEST_APPEND, aRv);
}


already_AddRefed<DOMRequest>
nsDOMDeviceStorage::AddOrAppendNamed(Blob* aBlob, const nsAString& aPath,
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

already_AddRefed<DOMRequest>
nsDOMDeviceStorage::CreateFileDescriptor(const nsAString& aPath,
                                         DeviceStorageFileDescriptor* aDSFileDescriptor,
                                         ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aDSFileDescriptor);

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

  if (IsFullPath(aPath)) {
    nsString storagePath;
    nsRefPtr<nsDOMDeviceStorage> ds = GetStorage(aPath, storagePath);
    if (!ds) {
      nsRefPtr<DOMRequest> request = new DOMRequest(win);
      r = new PostErrorEvent(request, POST_ERROR_EVENT_UNKNOWN);
      aRv = NS_DispatchToCurrentThread(r);
      if (aRv.Failed()) {
        return nullptr;
      }
      return request.forget();
    }
    return ds->CreateFileDescriptor(storagePath, aDSFileDescriptor, aRv);
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

  aRv = NS_DispatchToCurrentThread(r);
  if (aRv.Failed()) {
    return nullptr;
  }
  return request.forget();
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
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  PRTime since = 0;
  if (aOptions.mSince.WasPassed() && !aOptions.mSince.Value().IsUndefined()) {
    since = PRTime(aOptions.mSince.Value().TimeStamp().toDouble());
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

  if (DeviceStorageStatics::IsPromptTesting()) {
    r->Allow(JS::UndefinedHandleValue);
    return cursor.forget();
  }

  nsContentPermissionUtils::AskPermission(r, win);

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
  MOZ_ASSERT(NS_IsMainThread());
  Notify(aData.get(), aFile);
}

void
nsDOMDeviceStorage::OnDiskSpaceWatcher(bool aLowDiskSpace)
{
  MOZ_ASSERT(NS_IsMainThread());
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
  MOZ_ASSERT(NS_IsMainThread());

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
  MOZ_ASSERT(NS_IsMainThread());

  if (!aType.EqualsLiteral("change")) {
    return;
  }

  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (NS_WARN_IF(!win)) {
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
    aRv.Throw(rv);
  }
}
