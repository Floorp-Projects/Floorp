/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDeviceStorage.h"

#include "mozilla/Attributes.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/devicestorage/PDeviceStorageRequestChild.h"
#include "mozilla/dom/ipc/Blob.h"
#include "mozilla/dom/PBrowserChild.h"
#include "mozilla/dom/PContentPermissionRequestChild.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"

#include "nsAutoPtr.h"
#include "nsDOMEvent.h"
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
#include "DictionaryHelpers.h"
#include "nsContentUtils.h"
#include "nsXULAppAPI.h"
#include "TabChild.h"
#include "DeviceStorageRequestChild.h"
#include "nsIDOMDeviceStorageChangeEvent.h"
#include "nsCRT.h"
#include "nsIObserverService.h"
#include "GeneratedEvents.h"
#include "nsIMIMEService.h"
#include "nsCExternalHandlerService.h"
#include "nsIPermissionManager.h"
#include "nsIStringBundle.h"
#include <algorithm>

// Microsoft's API Name hackery sucks
#undef CreateEvent

#ifdef MOZ_WIDGET_GONK
#include "nsIVolume.h"
#include "nsIVolumeService.h"
#endif

#define DEVICESTORAGE_PROPERTIES "chrome://global/content/devicestorage.properties"
#define DEVICESTORAGE_PICTURES   "pictures"
#define DEVICESTORAGE_VIDEOS     "videos"
#define DEVICESTORAGE_MUSIC      "music"
#define DEVICESTORAGE_APPS       "apps"
#define DEVICESTORAGE_SDCARD     "sdcard"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::devicestorage;

#include "nsDirectoryServiceDefs.h"

nsAutoPtr<DeviceStorageTypeChecker> DeviceStorageTypeChecker::sDeviceStorageTypeChecker;

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

  NS_ASSERTION(NS_IsMainThread(), "This can only be created on the main thread!");

  nsCOMPtr<nsIStringBundleService> stringService = mozilla::services::GetStringBundleService();
  if (!stringService) {
    return nullptr;
  }

  nsCOMPtr<nsIStringBundle> filterBundle;
  if (NS_FAILED(stringService->CreateBundle(DEVICESTORAGE_PROPERTIES, getter_AddRefs(filterBundle)))) {
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
  aBundle->GetStringFromName(NS_ConvertASCIItoUTF16(DEVICESTORAGE_PICTURES).get(), getter_Copies(mPicturesExtensions));
  aBundle->GetStringFromName(NS_ConvertASCIItoUTF16(DEVICESTORAGE_MUSIC).get(), getter_Copies(mMusicExtensions));
  aBundle->GetStringFromName(NS_ConvertASCIItoUTF16(DEVICESTORAGE_VIDEOS).get(), getter_Copies(mVideosExtensions));
}


bool
DeviceStorageTypeChecker::Check(const nsAString& aType, nsIDOMBlob* aBlob)
{
  NS_ASSERTION(aBlob, "Calling Check without a blob");

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
      aType.EqualsLiteral(DEVICESTORAGE_SDCARD)) {
    // Apps and sdcard have no restriction on mime types
    return true;
  }

  return false;
}

bool
DeviceStorageTypeChecker::Check(const nsAString& aType, nsIFile* aFile)
{
  NS_ASSERTION(aFile, "Calling Check without a file");

  if (aType.EqualsLiteral(DEVICESTORAGE_APPS) ||
      aType.EqualsLiteral(DEVICESTORAGE_SDCARD)) {
    // apps have no restrictions on what file extensions used.
    return true;
  }

  nsString path;
  aFile->GetPath(path);

  int32_t dotIdx = path.RFindChar(PRUnichar('.'));
  if (dotIdx == kNotFound) {
    return false;
  }

  nsAutoString extensionMatch;
  extensionMatch.AssignLiteral("*");
  extensionMatch.Append(Substring(path, dotIdx));
  extensionMatch.AppendLiteral(";");

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

nsresult
DeviceStorageTypeChecker::GetPermissionForType(const nsAString& aType, nsACString& aPermissionResult)
{
  if (!aType.EqualsLiteral(DEVICESTORAGE_PICTURES) &&
      !aType.EqualsLiteral(DEVICESTORAGE_VIDEOS) &&
      !aType.EqualsLiteral(DEVICESTORAGE_MUSIC) &&
      !aType.EqualsLiteral(DEVICESTORAGE_APPS) &&
      !aType.EqualsLiteral(DEVICESTORAGE_SDCARD)) {
    // unknown type
    return NS_ERROR_FAILURE;
  }

  aPermissionResult.AssignLiteral("device-storage:");
  aPermissionResult.Append(NS_ConvertUTF16toUTF8(aType));
  return NS_OK;
}

nsresult
DeviceStorageTypeChecker::GetAccessForRequest(const DeviceStorageRequestType aRequestType, nsACString& aAccessResult)
{
  switch(aRequestType) {
    case DEVICE_STORAGE_REQUEST_READ:
    case DEVICE_STORAGE_REQUEST_WATCH:
    case DEVICE_STORAGE_REQUEST_FREE_SPACE:
    case DEVICE_STORAGE_REQUEST_USED_SPACE:
    case DEVICE_STORAGE_REQUEST_AVAILABLE:
      aAccessResult.AssignLiteral("read");
      break;
    case DEVICE_STORAGE_REQUEST_WRITE:
    case DEVICE_STORAGE_REQUEST_DELETE:
      aAccessResult.AssignLiteral("write");
      break;
    case DEVICE_STORAGE_REQUEST_CREATE:
      aAccessResult.AssignLiteral("create");
      break;
    default:
      aAccessResult.AssignLiteral("undefined");
  }
  return NS_OK;
}

bool
DeviceStorageTypeChecker::IsVolumeBased(const nsAString& aType)
{
  // The apps aren't stored in the same place as the media, so
  // we only ever return a single apps object, and not an array
  // with one per volume (as is the case for the remaining
  // storage types).
  return !aType.EqualsLiteral(DEVICESTORAGE_APPS);
}

NS_IMPL_ISUPPORTS1(FileUpdateDispatcher, nsIObserver)

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
                              const PRUnichar *aData)
{
  if (XRE_GetProcessType() != GeckoProcessType_Default) {

    DeviceStorageFile* file = static_cast<DeviceStorageFile*>(aSubject);
    if (!file || !file->mFile) {
      NS_WARNING("Device storage file looks invalid!");
      return NS_OK;
    }

    nsString fullpath;
    nsresult rv = file->mFile->GetPath(fullpath);
    if (NS_FAILED(rv)) {
      NS_WARNING("Could not get path from the nsIFile!");
      return NS_OK;
    }

    ContentChild::GetSingleton()->SendFilePathUpdateNotify(file->mStorageType,
                                                           fullpath,
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
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    nsString data;
    CopyASCIItoUTF16(mType, data);
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();

    obs->NotifyObservers(mFile, "file-watcher-notify", data.get());
    return NS_OK;
  }

private:
  nsRefPtr<DeviceStorageFile> mFile;
  nsCString mType;
};

DeviceStorageFile::DeviceStorageFile(const nsAString& aStorageType,
                                     nsIFile* aFile,
                                     const nsAString& aPath)
  : mPath(aPath)
  , mStorageType(aStorageType)
  , mEditable(false)
{
  NS_ASSERTION(aFile, "Must not create a DeviceStorageFile with a null nsIFile");
  // always take a clone
  nsCOMPtr<nsIFile> file;
  aFile->Clone(getter_AddRefs(mFile));

  AppendRelativePath();
  NormalizeFilePath();

  DebugOnly<DeviceStorageTypeChecker*> typeChecker = DeviceStorageTypeChecker::CreateOrGet();
  NS_ASSERTION(typeChecker, "DeviceStorageTypeChecker is null");
}

DeviceStorageFile::DeviceStorageFile(const nsAString& aStorageType, nsIFile* aFile)
  : mStorageType(aStorageType)
  , mEditable(false)
{
  NS_ASSERTION(aFile, "Must not create a DeviceStorageFile with a null nsIFile");
  // always take a clone
  nsCOMPtr<nsIFile> file;
  aFile->Clone(getter_AddRefs(mFile));

  DebugOnly<DeviceStorageTypeChecker*> typeChecker = DeviceStorageTypeChecker::CreateOrGet();
  NS_ASSERTION(typeChecker, "DeviceStorageTypeChecker is null");
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
  nsAString::const_iterator start, end;
  mPath.BeginReading(start);
  mPath.EndReading(end);

  // if the path has a ~ or \ in it, return false.
  NS_NAMED_LITERAL_STRING(tilde, "~");
  NS_NAMED_LITERAL_STRING(bslash, "\\");
  if (FindInReadable(tilde, start, end) ||
      FindInReadable(bslash, start, end)) {
    return false;
   }
  // split on /.  if any token is "", ., or .., return false.
  NS_ConvertUTF16toUTF8 cname(mPath);
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
#if defined(XP_WIN)
  PRUnichar* cur = mPath.BeginWriting();
  PRUnichar* end = mPath.EndWriting();
  for (; cur < end; ++cur) {
    if (PRUnichar('\\') == *cur)
      *cur = PRUnichar('/');
  }
#endif
}

void
DeviceStorageFile::AppendRelativePath() {
#if defined(XP_WIN)
  // replace forward slashes with backslashes,
  // since nsLocalFileWin chokes on them
  nsString temp;
  temp.Assign(mPath);

  PRUnichar* cur = temp.BeginWriting();
  PRUnichar* end = temp.EndWriting();

  for (; cur < end; ++cur) {
    if (PRUnichar('/') == *cur)
      *cur = PRUnichar('\\');
  }
  mFile->AppendRelativePath(temp);
#else
  mFile->AppendRelativePath(mPath);
#endif
}

nsresult
DeviceStorageFile::Write(nsIInputStream* aInputStream)
{
  if (!aInputStream) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = mFile->Create(nsIFile::NORMAL_FILE_TYPE, 00600);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<IOEventComplete> iocomplete = new IOEventComplete(this, "created");
  NS_DispatchToMainThread(iocomplete);

  uint64_t bufSize = 0;
  aInputStream->Available(&bufSize);

  nsCOMPtr<nsIOutputStream> outputStream;
  NS_NewLocalFileOutputStream(getter_AddRefs(outputStream), mFile);

  if (!outputStream) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIOutputStream> bufferedOutputStream;
  rv = NS_NewBufferedOutputStream(getter_AddRefs(bufferedOutputStream),
                                  outputStream,
                                  4096*4);
  NS_ENSURE_SUCCESS(rv, rv);

  while (bufSize) {
    uint32_t wrote;
    rv = bufferedOutputStream->WriteFrom(aInputStream,
                                         static_cast<uint32_t>(std::min<uint64_t>(bufSize, UINT32_MAX)),
                                         &wrote);
    if (NS_FAILED(rv)) {
      break;
    }
    bufSize -= wrote;
  }

  iocomplete = new IOEventComplete(this, "modified");
  NS_DispatchToMainThread(iocomplete);

  bufferedOutputStream->Close();
  outputStream->Close();
  if (NS_FAILED(rv)) {
    return rv;
  }
  return NS_OK;
}

nsresult
DeviceStorageFile::Write(InfallibleTArray<uint8_t>& aBits) {

  nsresult rv = mFile->Create(nsIFile::NORMAL_FILE_TYPE, 00600);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<IOEventComplete> iocomplete = new IOEventComplete(this, "created");
  NS_DispatchToMainThread(iocomplete);

  nsCOMPtr<nsIOutputStream> outputStream;
  NS_NewLocalFileOutputStream(getter_AddRefs(outputStream), mFile);

  if (!outputStream) {
    return NS_ERROR_FAILURE;
  }

  uint32_t wrote;
  outputStream->Write((char*) aBits.Elements(), aBits.Length(), &wrote);
  outputStream->Close();

  iocomplete = new IOEventComplete(this, "modified");
  NS_DispatchToMainThread(iocomplete);

  if (aBits.Length() != wrote) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
DeviceStorageFile::Remove()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  bool check;
  nsresult rv = mFile->Exists(&check);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!check) {
    return NS_OK;
  }

  rv = mFile->Remove(true);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<IOEventComplete> iocomplete = new IOEventComplete(this, "deleted");
  NS_DispatchToMainThread(iocomplete);
  return NS_OK;
}

void
DeviceStorageFile::CollectFiles(nsTArray<nsRefPtr<DeviceStorageFile> > &aFiles,
                                PRTime aSince)
{
  nsString rootPath;
  nsresult rv = mFile->GetPath(rootPath);
  if (NS_FAILED(rv)) {
    return;
  }

  return collectFilesInternal(aFiles, aSince, rootPath);
}

void
DeviceStorageFile::collectFilesInternal(nsTArray<nsRefPtr<DeviceStorageFile> > &aFiles,
                                        PRTime aSince,
                                        nsAString& aRootPath)
{
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
      DeviceStorageFile dsf(mStorageType, f);
      dsf.SetPath(newPath);
      dsf.collectFilesInternal(aFiles, aSince, aRootPath);
    } else if (isFile) {
      nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType, f);
      dsf->SetPath(newPath);
      aFiles.AppendElement(dsf);
    }
  }
}

void
DeviceStorageFile::DirectoryDiskUsage(nsIFile* aFile, uint64_t* aSoFar, const nsAString& aStorageType)
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
  NS_ASSERTION(files, "GetDirectoryEntries must return a nsIDirectoryEnumerator");

  DeviceStorageTypeChecker* typeChecker = DeviceStorageTypeChecker::CreateOrGet();
  if (!typeChecker) {
    return;
  }

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
      DirectoryDiskUsage(f, aSoFar, aStorageType);
    } else if (isFile) {

      if (!typeChecker->Check(aStorageType, f)) {
        continue;
      }

      int64_t size;
      rv = f->GetFileSize(&size);
      if (NS_SUCCEEDED(rv)) {
        *aSoFar += size;
      }
    }
  }
}

NS_IMPL_THREADSAFE_ISUPPORTS0(DeviceStorageFile)

#ifdef MOZ_WIDGET_GONK
nsresult
GetSDCardStatus(nsAString& aPath, nsAString& aState) {
  nsCOMPtr<nsIVolumeService> vs = do_GetService(NS_VOLUMESERVICE_CONTRACTID);
  if (!vs) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIVolume> vol;
  vs->GetVolumeByPath(aPath, getter_AddRefs(vol));
  if (!vol) {
    return NS_ERROR_FAILURE;
  }

  int32_t state;
  nsresult rv = vol->GetState(&state);
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  if (state == nsIVolume::STATE_MOUNTED) {
    aState.AssignLiteral("available");
  } else if (state == nsIVolume::STATE_SHARED || state == nsIVolume::STATE_SHAREDMNT) {
    aState.AssignLiteral("shared");
  } else {
    aState.AssignLiteral("unavailable");
  }
  return NS_OK;
}

static void
RegisterForSDCardChanges(nsIObserver* aObserver)
{
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  obs->AddObserver(aObserver, NS_VOLUME_STATE_CHANGED, false);
}

static void
UnregisterForSDCardChanges(nsIObserver* aObserver)
{
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  obs->RemoveObserver(aObserver, NS_VOLUME_STATE_CHANGED);
}
#endif

void
nsDOMDeviceStorage::SetRootDirectoryForType(const nsAString& aType, const nsAString& aVolName)
{
  nsCOMPtr<nsIFile> f;
  nsCOMPtr<nsIProperties> dirService = do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
  NS_ASSERTION(dirService, "Must have directory service");

  mVolumeName = NS_LITERAL_STRING("");
#ifdef MOZ_WIDGET_GONK
  nsString volMountPoint(NS_LITERAL_STRING("/sdcard"));
  if (!aVolName.EqualsLiteral("")) {
    nsCOMPtr<nsIVolumeService> vs = do_GetService(NS_VOLUMESERVICE_CONTRACTID);
    if (vs) {
      nsresult rv;
      nsCOMPtr<nsIVolume> vol;
      rv = vs->GetVolumeByName(aVolName, getter_AddRefs(vol));
      if (NS_SUCCEEDED(rv)) {
        vol->GetMountPoint(volMountPoint);
        mVolumeName = aVolName;
      }
    }
  }
#endif

  // Picture directory
  if (aType.EqualsLiteral(DEVICESTORAGE_PICTURES)) {
#ifdef MOZ_WIDGET_GONK
    NS_NewLocalFile(volMountPoint, false, getter_AddRefs(f));
#elif defined (MOZ_WIDGET_COCOA)
    dirService->Get(NS_OSX_PICTURE_DOCUMENTS_DIR, NS_GET_IID(nsIFile), getter_AddRefs(f));
#elif defined (XP_UNIX)
    dirService->Get(NS_UNIX_XDG_PICTURES_DIR, NS_GET_IID(nsIFile), getter_AddRefs(f));
#elif defined (XP_WIN)
    dirService->Get(NS_WIN_PICTURES_DIR, NS_GET_IID(nsIFile), getter_AddRefs(f));
#endif
  }

  // Video directory
  else if (aType.EqualsLiteral(DEVICESTORAGE_VIDEOS)) {
#ifdef MOZ_WIDGET_GONK
    NS_NewLocalFile(volMountPoint, false, getter_AddRefs(f));
#elif defined (MOZ_WIDGET_COCOA)
    dirService->Get(NS_OSX_MOVIE_DOCUMENTS_DIR, NS_GET_IID(nsIFile), getter_AddRefs(f));
#elif defined (XP_UNIX)
    dirService->Get(NS_UNIX_XDG_VIDEOS_DIR, NS_GET_IID(nsIFile), getter_AddRefs(f));
#elif defined (XP_WIN)
    dirService->Get(NS_WIN_VIDEOS_DIR, NS_GET_IID(nsIFile), getter_AddRefs(f));
#endif
  }

  // Music directory
  else if (aType.EqualsLiteral(DEVICESTORAGE_MUSIC)) {
#ifdef MOZ_WIDGET_GONK
    NS_NewLocalFile(volMountPoint, false, getter_AddRefs(f));
#elif defined (MOZ_WIDGET_COCOA)
    dirService->Get(NS_OSX_MUSIC_DOCUMENTS_DIR, NS_GET_IID(nsIFile), getter_AddRefs(f));
#elif defined (XP_UNIX)
    dirService->Get(NS_UNIX_XDG_MUSIC_DIR, NS_GET_IID(nsIFile), getter_AddRefs(f));
#elif defined (XP_WIN)
    dirService->Get(NS_WIN_MUSIC_DIR, NS_GET_IID(nsIFile), getter_AddRefs(f));
#endif
  }

  // Apps directory
  else if (aType.EqualsLiteral(DEVICESTORAGE_APPS)) {
#ifdef MOZ_WIDGET_GONK
    NS_NewLocalFile(NS_LITERAL_STRING("/data"), false, getter_AddRefs(f));
#else
    dirService->Get(NS_APP_USER_PROFILE_50_DIR, NS_GET_IID(nsIFile), getter_AddRefs(f));
    if (f) {
      f->AppendRelativeNativePath(NS_LITERAL_CSTRING("webapps"));
    }
#endif
  }

   // default SDCard
   else if (aType.EqualsLiteral(DEVICESTORAGE_SDCARD)) {
#ifdef MOZ_WIDGET_GONK
     NS_NewLocalFile(volMountPoint, false, getter_AddRefs(f));
#else
    // Eventually, on desktop, we want to do something smarter -- for example,
    // detect when an sdcard is inserted, and use that instead of this.
    dirService->Get(NS_APP_USER_PROFILE_50_DIR, NS_GET_IID(nsIFile), getter_AddRefs(f));
    if (f) {
      f->AppendRelativeNativePath(NS_LITERAL_CSTRING("fake-sdcard"));
    }
#endif
  }

  // in testing, we default all device storage types to a temp directory
  if (f && mozilla::Preferences::GetBool("device.storage.testing", false)) {
    dirService->Get(NS_OS_TEMP_DIR, NS_GET_IID(nsIFile), getter_AddRefs(f));
    if (f) {
      f->AppendRelativeNativePath(NS_LITERAL_CSTRING("device-storage-testing"));
      f->Create(nsIFile::DIRECTORY_TYPE, 0777);
      f->Normalize();
    }
  }

#ifdef MOZ_WIDGET_GONK
  RegisterForSDCardChanges(this);
#endif

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  obs->AddObserver(this, "file-watcher-update", false);
  mRootDirectory = f;
  mStorageType = aType;
}

jsval InterfaceToJsval(nsPIDOMWindow* aWindow, nsISupports* aObject, const nsIID* aIID)
{
  nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(aWindow);
  if (!sgo) {
    return JSVAL_NULL;
  }

  nsIScriptContext *scriptContext = sgo->GetScriptContext();
  if (!scriptContext) {
    return JSVAL_NULL;
  }

  AutoPushJSContext cx(scriptContext->GetNativeContext());
  if (!cx) {
    return JSVAL_NULL;
  }

  jsval someJsVal;
  nsresult rv = nsContentUtils::WrapNative(cx,
                                           JS_GetGlobalObject(cx),
                                           aObject,
                                           aIID,
                                           &someJsVal);
  if (NS_FAILED(rv)) {
    return JSVAL_NULL;
  }

  return someJsVal;
}

jsval nsIFileToJsval(nsPIDOMWindow* aWindow, DeviceStorageFile* aFile)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aWindow, "Null Window");

  if (!aFile) {
    return JSVAL_NULL;
  }

  if (aFile->mEditable) {
    // TODO - needs janv's file handle support.
    return JSVAL_NULL;
  }

  nsCOMPtr<nsIDOMBlob> blob = new nsDOMFileFile(aFile->mFile, aFile->mPath,
                                                EmptyString());
  return InterfaceToJsval(aWindow, blob, &NS_GET_IID(nsIDOMBlob));
}

jsval StringToJsval(nsPIDOMWindow* aWindow, nsAString& aString)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aWindow, "Null Window");

  nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(aWindow);
  if (!sgo) {
    return JSVAL_NULL;
  }

  nsIScriptContext *scriptContext = sgo->GetScriptContext();
  if (!scriptContext) {
    return JSVAL_NULL;
  }

  AutoPushJSContext cx(scriptContext->GetNativeContext());
  if (!cx) {
    return JSVAL_NULL;
  }

  JSAutoRequest ar(cx);

  jsval result = JSVAL_NULL;
  if (!xpc::StringToJsval(cx, aString, &result)) {
    return JSVAL_NULL;
  }

  return result;
}

class DeviceStorageCursorRequest MOZ_FINAL
  : public nsIContentPermissionRequest
  , public PCOMContentPermissionRequestChild
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(DeviceStorageCursorRequest, nsIContentPermissionRequest)

  NS_FORWARD_NSICONTENTPERMISSIONREQUEST(mCursor->);

  DeviceStorageCursorRequest(nsDOMDeviceStorageCursor* aCursor)
    : mCursor(aCursor) { }

  ~DeviceStorageCursorRequest() {}

  bool Recv__delete__(const bool& allow)
  {
    if (allow) {
      Allow();
    }
    else {
      Cancel();
    }
    return true;
  }

  void IPDLRelease()
  {
    Release();
  }

private:
  nsRefPtr<nsDOMDeviceStorageCursor> mCursor;
};

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DeviceStorageCursorRequest)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContentPermissionRequest)
  NS_INTERFACE_MAP_ENTRY(nsIContentPermissionRequest)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DeviceStorageCursorRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DeviceStorageCursorRequest)

NS_IMPL_CYCLE_COLLECTION_1(DeviceStorageCursorRequest,
                           mCursor)


class PostErrorEvent : public nsRunnable
{
public:
  PostErrorEvent(nsRefPtr<DOMRequest>& aRequest, const char* aMessage)
  {
    mRequest.swap(aRequest);
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
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    mRequest->FireError(mError);
    mRequest = nullptr;
    return NS_OK;
  }

private:
  nsRefPtr<DOMRequest> mRequest;
  nsString mError;
};

ContinueCursorEvent::ContinueCursorEvent(nsRefPtr<DOMRequest>& aRequest)
{
  mRequest.swap(aRequest);
}

ContinueCursorEvent::ContinueCursorEvent(DOMRequest* aRequest)
  : mRequest(aRequest)
{
}

already_AddRefed<DeviceStorageFile>
ContinueCursorEvent::GetNextFile()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsDOMDeviceStorageCursor* cursor = static_cast<nsDOMDeviceStorageCursor*>(mRequest.get());
  nsString cursorStorageType;
  cursor->GetStorageType(cursorStorageType);

  DeviceStorageTypeChecker* typeChecker = DeviceStorageTypeChecker::CreateOrGet();
  if (!typeChecker) {
    return nullptr;
  }

  while (cursor->mFiles.Length() > 0) {
    nsRefPtr<DeviceStorageFile> file = cursor->mFiles[0];
    cursor->mFiles.RemoveElementAt(0);
    if (!typeChecker->Check(cursorStorageType, file->mFile)) {
      continue;
    }
    return file.forget();
  }

  return nullptr;
}

ContinueCursorEvent::~ContinueCursorEvent() {}

void
ContinueCursorEvent::Continue()
{
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    NS_DispatchToMainThread(this);
    return;
  }

  nsRefPtr<DeviceStorageFile> file = GetNextFile();

  if (!file) {
    // done with enumeration.
    NS_DispatchToMainThread(this);
    return;
  }

  nsString fullpath;
  nsresult rv = file->mFile->GetPath(fullpath);
  if (NS_FAILED(rv)) {
    NS_ASSERTION(false, "GetPath failed to return a valid path");
    return;
  }

  nsDOMDeviceStorageCursor* cursor = static_cast<nsDOMDeviceStorageCursor*>(mRequest.get());
  nsString cursorStorageType;
  cursor->GetStorageType(cursorStorageType);

  DeviceStorageRequestChild* child = new DeviceStorageRequestChild(mRequest, file);
  child->SetCallback(cursor);
  DeviceStorageGetParams params(cursorStorageType, file->mPath, fullpath);
  ContentChild::GetSingleton()->SendPDeviceStorageRequestConstructor(child, params);
  mRequest = nullptr;
}

NS_IMETHODIMP
ContinueCursorEvent::Run()
{
  nsRefPtr<DeviceStorageFile> file = GetNextFile();

  nsDOMDeviceStorageCursor* cursor = static_cast<nsDOMDeviceStorageCursor*>(mRequest.get());
  jsval val = nsIFileToJsval(cursor->GetOwner(), file);

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
    NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

    bool check;
    mFile->mFile->IsDirectory(&check);
    if (!check) {
      nsCOMPtr<PostErrorEvent> event = new PostErrorEvent(mRequest, POST_ERROR_EVENT_FILE_NOT_ENUMERABLE);
      NS_DispatchToMainThread(event);
      return NS_OK;
    }

    nsDOMDeviceStorageCursor* cursor = static_cast<nsDOMDeviceStorageCursor*>(mRequest.get());
    mFile->CollectFiles(cursor->mFiles, cursor->mSince);

    nsCOMPtr<ContinueCursorEvent> event = new ContinueCursorEvent(mRequest);
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

nsDOMDeviceStorageCursor::nsDOMDeviceStorageCursor(nsIDOMWindow* aWindow,
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
nsDOMDeviceStorageCursor::GetType(nsACString & aType)
{
  return DeviceStorageTypeChecker::GetPermissionForType(mFile->mStorageType, aType);
}

NS_IMETHODIMP
nsDOMDeviceStorageCursor::GetAccess(nsACString & aAccess)
{
  aAccess = NS_LITERAL_CSTRING("read");
  return NS_OK;
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
  nsCOMPtr<PostErrorEvent> event = new PostErrorEvent(this, POST_ERROR_EVENT_PERMISSION_DENIED);
  NS_DispatchToMainThread(event);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceStorageCursor::Allow()
{
  if (!mFile->IsSafePath()) {
    nsCOMPtr<nsIRunnable> r = new PostErrorEvent(this, POST_ERROR_EVENT_PERMISSION_DENIED);
    NS_DispatchToMainThread(r);
    return NS_OK;
  }

  if (XRE_GetProcessType() != GeckoProcessType_Default) {

    nsString fullpath;
    nsresult rv = mFile->mFile->GetPath(fullpath);

    if (NS_FAILED(rv)) {
      // just do nothing
      return NS_OK;
    }

    PDeviceStorageRequestChild* child = new DeviceStorageRequestChild(this, mFile);
    DeviceStorageEnumerationParams params(mFile->mStorageType, fullpath, mSince);
    ContentChild::GetSingleton()->SendPDeviceStorageRequestConstructor(child, params);
    return NS_OK;
  }

  nsCOMPtr<nsIEventTarget> target = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  NS_ASSERTION(target, "Must have stream transport service");

  nsCOMPtr<InitCursorEvent> event = new InitCursorEvent(this, mFile);
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

  if (mRooted) {
    // We call onsuccess multiple times. clear the last
    // rooted result.
    mResult = JSVAL_VOID;
    NS_DROP_JS_OBJECTS(this, nsDOMDeviceStorageCursor);
    mDone = false;
    mRooted = false;
  }

  nsCOMPtr<ContinueCursorEvent> event = new ContinueCursorEvent(this);
  event->Continue();

  mOkToCallContinue = false;
}

bool
nsDOMDeviceStorageCursor::Recv__delete__(const bool& allow)
{
  if (allow) {
    Allow();
  }
  else {
    Cancel();
  }
  return true;
}

void
nsDOMDeviceStorageCursor::IPDLRelease()
{
  Release();
}

void
nsDOMDeviceStorageCursor::RequestComplete()
{
  NS_ASSERTION(!mOkToCallContinue, "mOkToCallContinue must be false");
  mOkToCallContinue = true;
}

class PostAvailableResultEvent : public nsRunnable
{
public:
  PostAvailableResultEvent(const nsAString& aPath, DOMRequest* aRequest)
    : mPath(aPath)
    , mRequest(aRequest)
  {
  }

  ~PostAvailableResultEvent() {}

  NS_IMETHOD Run()
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

    nsString state;
    state.Assign(NS_LITERAL_STRING("available"));
#ifdef MOZ_WIDGET_GONK
    nsresult rv = GetSDCardStatus(mPath, state);
    if (NS_FAILED(rv)) {
      state.Assign(NS_LITERAL_STRING("unavailable"));
    }
#endif

    jsval result = StringToJsval(mRequest->GetOwner(), state);
    mRequest->FireSuccess(result);
    mRequest = nullptr;
    return NS_OK;
  }

private:
  nsString mPath;
  nsRefPtr<DOMRequest> mRequest;
};

class PostResultEvent : public nsRunnable
{
public:
  PostResultEvent(nsRefPtr<DOMRequest>& aRequest, DeviceStorageFile* aFile)
    : mFile(aFile)
    {
      mRequest.swap(aRequest);
    }

  PostResultEvent(nsRefPtr<DOMRequest>& aRequest, const nsAString & aPath)
    : mPath(aPath)
    {
      mRequest.swap(aRequest);
    }

  PostResultEvent(nsRefPtr<DOMRequest>& aRequest, const int64_t aValue)
    : mValue(aValue)
    {
      mRequest.swap(aRequest);
    }

  ~PostResultEvent() {}

  NS_IMETHOD Run()
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

    jsval result = JSVAL_NULL;
    nsPIDOMWindow* window = mRequest->GetOwner();

    if (mFile) {
      result = nsIFileToJsval(window, mFile);
    } else if (mPath.Length()) {
      result = StringToJsval(window, mPath);
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
  int64_t mValue;
  nsRefPtr<DOMRequest> mRequest;
};

class WriteFileEvent : public nsRunnable
{
public:
  WriteFileEvent(nsIDOMBlob* aBlob,
                 DeviceStorageFile *aFile,
                 nsRefPtr<DOMRequest>& aRequest)
  : mBlob(aBlob)
  , mFile(aFile)
    {
      mRequest.swap(aRequest);
    }

  ~WriteFileEvent() {}

  NS_IMETHOD Run()
  {
    NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

    nsCOMPtr<nsIInputStream> stream;
    mBlob->GetInternalStream(getter_AddRefs(stream));

    bool check = false;
    mFile->mFile->Exists(&check);
    if (check) {
      nsCOMPtr<PostErrorEvent> event = new PostErrorEvent(mRequest, POST_ERROR_EVENT_FILE_EXISTS);
      NS_DispatchToMainThread(event);
      return NS_OK;
    }

    nsresult rv = mFile->Write(stream);

    if (NS_FAILED(rv)) {
      mFile->mFile->Remove(false);

      nsCOMPtr<PostErrorEvent> event = new PostErrorEvent(mRequest, POST_ERROR_EVENT_UNKNOWN);
      NS_DispatchToMainThread(event);
      return NS_OK;
    }

    nsCOMPtr<PostResultEvent> event = new PostResultEvent(mRequest, mFile->mPath);
    NS_DispatchToMainThread(event);
    return NS_OK;
  }

private:
  nsCOMPtr<nsIDOMBlob> mBlob;
  nsRefPtr<DeviceStorageFile> mFile;
  nsRefPtr<DOMRequest> mRequest;
};

class ReadFileEvent : public nsRunnable
{
public:
    ReadFileEvent(DeviceStorageFile* aFile,
                  nsRefPtr<DOMRequest>& aRequest)
  : mFile(aFile)
    {
      mRequest.swap(aRequest);
    }

  ~ReadFileEvent() {}

  NS_IMETHOD Run()
  {
    NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

    nsRefPtr<nsRunnable> r;
    if (!mFile->mEditable) {
      bool check = false;
      mFile->mFile->Exists(&check);
      if (!check) {
        r = new PostErrorEvent(mRequest, POST_ERROR_EVENT_FILE_DOES_NOT_EXIST);
      }
    }

    if (!r) {
      r = new PostResultEvent(mRequest, mFile);
    }
    NS_DispatchToMainThread(r);
    return NS_OK;
  }

private:
  nsRefPtr<DeviceStorageFile> mFile;
  nsRefPtr<DOMRequest> mRequest;
};

class DeleteFileEvent : public nsRunnable
{
public:
  DeleteFileEvent(DeviceStorageFile* aFile,
                  nsRefPtr<DOMRequest>& aRequest)
  : mFile(aFile)
    {
      mRequest.swap(aRequest);
    }

  ~DeleteFileEvent() {}

  NS_IMETHOD Run()
  {
    NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
    mFile->Remove();

    nsRefPtr<nsRunnable> r;
    bool check = false;
    mFile->mFile->Exists(&check);
    if (check) {
      r = new PostErrorEvent(mRequest, POST_ERROR_EVENT_FILE_DOES_NOT_EXIST);
    }
    else {
      r = new PostResultEvent(mRequest, mFile->mPath);
    }
    NS_DispatchToMainThread(r);
    return NS_OK;
  }

private:
  nsRefPtr<DeviceStorageFile> mFile;
  nsRefPtr<DOMRequest> mRequest;
};

class UsedSpaceFileEvent : public nsRunnable
{
public:
  UsedSpaceFileEvent(DeviceStorageFile* aFile, nsRefPtr<DOMRequest>& aRequest)
  : mFile(aFile)
    {
      mRequest.swap(aRequest);
    }

  ~UsedSpaceFileEvent() {}

  NS_IMETHOD Run()
  {
    NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
    nsCOMPtr<nsIRunnable> r;
    uint64_t diskUsage = 0;
    DeviceStorageFile::DirectoryDiskUsage(mFile->mFile, &diskUsage, mFile->mStorageType);

    r = new PostResultEvent(mRequest, diskUsage);
    NS_DispatchToMainThread(r);
    return NS_OK;
  }

private:
  nsRefPtr<DeviceStorageFile> mFile;
  nsRefPtr<DOMRequest> mRequest;
};

class FreeSpaceFileEvent : public nsRunnable
{
public:
  FreeSpaceFileEvent(DeviceStorageFile* aFile, nsRefPtr<DOMRequest>& aRequest)
  : mFile(aFile)
    {
      mRequest.swap(aRequest);
    }

  ~FreeSpaceFileEvent() {}

  NS_IMETHOD Run()
  {
    NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
    nsCOMPtr<nsIRunnable> r;
    int64_t freeSpace = 0;
    nsresult rv = mFile->mFile->GetDiskSpaceAvailable(&freeSpace);
    if (NS_FAILED(rv)) {
      freeSpace = 0;
    }

    r = new PostResultEvent(mRequest, freeSpace);
    NS_DispatchToMainThread(r);
    return NS_OK;
  }

private:
  nsRefPtr<DeviceStorageFile> mFile;
  nsRefPtr<DOMRequest> mRequest;
};

class DeviceStorageRequest MOZ_FINAL
  : public nsIContentPermissionRequest
  , public nsIRunnable
  , public PCOMContentPermissionRequestChild
{
public:

    DeviceStorageRequest(const DeviceStorageRequestType aRequestType,
                         nsPIDOMWindow *aWindow,
                         nsIPrincipal *aPrincipal,
                         DeviceStorageFile *aFile,
                         DOMRequest* aRequest,
                         nsDOMDeviceStorage *aDeviceStorage,
                         nsIDOMEventListener *aListener)
      : mRequestType(aRequestType)
      , mWindow(aWindow)
      , mPrincipal(aPrincipal)
      , mFile(aFile)
      , mRequest(aRequest)
      , mDeviceStorage(aDeviceStorage)
      , mListener(aListener) {}

    DeviceStorageRequest(const DeviceStorageRequestType aRequestType,
                         nsPIDOMWindow *aWindow,
                         nsIPrincipal *aPrincipal,
                         DeviceStorageFile *aFile,
                         DOMRequest* aRequest,
                         nsIDOMBlob *aBlob = nullptr)
      : mRequestType(aRequestType)
      , mWindow(aWindow)
      , mPrincipal(aPrincipal)
      , mFile(aFile)
      , mRequest(aRequest)
      , mBlob(aBlob) {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(DeviceStorageRequest, nsIContentPermissionRequest)

  NS_IMETHOD Run() {

    if (mozilla::Preferences::GetBool("device.storage.prompt.testing", false)) {
      Allow();
      return NS_OK;
    }

    if (XRE_GetProcessType() == GeckoProcessType_Content) {

      // because owner implements nsITabChild, we can assume that it is
      // the one and only TabChild.
      TabChild* child = GetTabChildFrom(mWindow->GetDocShell());
      if (!child) {
        return NS_OK;
      }

      // Retain a reference so the object isn't deleted without IPDL's knowledge.
      // Corresponding release occurs in DeallocPContentPermissionRequest.
      AddRef();

      nsCString type;
      nsresult rv = DeviceStorageTypeChecker::GetPermissionForType(mFile->mStorageType, type);
      if (NS_FAILED(rv)) {
        return rv;
      }
      nsCString access;
      rv = DeviceStorageTypeChecker::GetAccessForRequest(DeviceStorageRequestType(mRequestType), access);
      if (NS_FAILED(rv)) {
        return rv;
      }
      child->SendPContentPermissionRequestConstructor(this, type, access, IPC::Principal(mPrincipal));

      Sendprompt();
      return NS_OK;
    }

    nsCOMPtr<nsIContentPermissionPrompt> prompt = do_CreateInstance(NS_CONTENT_PERMISSION_PROMPT_CONTRACTID);
    if (prompt) {
      prompt->Prompt(this);
    }
    return NS_OK;
  }

  NS_IMETHOD GetType(nsACString & aType)
  {
    nsCString type;
    nsresult rv = DeviceStorageTypeChecker::GetPermissionForType(mFile->mStorageType, aType);
    if (NS_FAILED(rv)) {
      return rv;
    }
    return NS_OK;
  }

  NS_IMETHOD GetAccess(nsACString & aAccess)
  {
    nsresult rv = DeviceStorageTypeChecker::GetAccessForRequest(DeviceStorageRequestType(mRequestType), aAccess);
    if (NS_FAILED(rv)) {
      return rv;
    }
    return NS_OK;
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
    nsCOMPtr<PostErrorEvent> event = new PostErrorEvent(mRequest, POST_ERROR_EVENT_PERMISSION_DENIED);
    NS_DispatchToMainThread(event);
    return NS_OK;
  }

  NS_IMETHOD Allow()
  {
    nsCOMPtr<nsIRunnable> r;

    if (!mRequest) {
      return NS_ERROR_FAILURE;
    }

    nsString fullpath;
    nsresult rv = mFile->mFile->GetPath(fullpath);

    if (NS_FAILED(rv)) {
      // just do nothing
      return NS_OK;
    }

    switch(mRequestType) {
      case DEVICE_STORAGE_REQUEST_CREATE:
      {
        if (!mBlob) {
          return NS_ERROR_FAILURE;
        }

        DeviceStorageTypeChecker* typeChecker = DeviceStorageTypeChecker::CreateOrGet();
        if (!typeChecker) {
          return NS_OK;
        }

        if (!typeChecker->Check(mFile->mStorageType, mFile->mFile) ||
            !typeChecker->Check(mFile->mStorageType, mBlob)) {
          r = new PostErrorEvent(mRequest, POST_ERROR_EVENT_ILLEGAL_TYPE);
          NS_DispatchToMainThread(r);
          return NS_OK;
        }

        if (XRE_GetProcessType() != GeckoProcessType_Default) {

          BlobChild* actor = ContentChild::GetSingleton()->GetOrCreateActorForBlob(mBlob);
          if (!actor) {
            return NS_ERROR_FAILURE;
          }

          DeviceStorageAddParams params;
          params.blobChild() = actor;
          params.type() = mFile->mStorageType;
          params.name() = mFile->mPath;
          params.fullpath() = fullpath;

          PDeviceStorageRequestChild* child = new DeviceStorageRequestChild(mRequest, mFile);
          ContentChild::GetSingleton()->SendPDeviceStorageRequestConstructor(child, params);
          return NS_OK;
        }
        r = new WriteFileEvent(mBlob, mFile, mRequest);
        break;
      }

      case DEVICE_STORAGE_REQUEST_READ:
      case DEVICE_STORAGE_REQUEST_WRITE:
      {
        DeviceStorageTypeChecker* typeChecker = DeviceStorageTypeChecker::CreateOrGet();
        if (!typeChecker) {
          return NS_OK;
        }

        if (!typeChecker->Check(mFile->mStorageType, mFile->mFile)) {
          r = new PostErrorEvent(mRequest, POST_ERROR_EVENT_ILLEGAL_TYPE);
          NS_DispatchToMainThread(r);
          return NS_OK;
        }

        if (XRE_GetProcessType() != GeckoProcessType_Default) {
          PDeviceStorageRequestChild* child = new DeviceStorageRequestChild(mRequest, mFile);
          DeviceStorageGetParams params(mFile->mStorageType, mFile->mPath, fullpath);
          ContentChild::GetSingleton()->SendPDeviceStorageRequestConstructor(child, params);
          return NS_OK;
        }

        r = new ReadFileEvent(mFile, mRequest);
        break;
      }

      case DEVICE_STORAGE_REQUEST_DELETE:
      {
        DeviceStorageTypeChecker* typeChecker = DeviceStorageTypeChecker::CreateOrGet();
        if (!typeChecker) {
          return NS_OK;
        }

        if (!typeChecker->Check(mFile->mStorageType, mFile->mFile)) {
          r = new PostErrorEvent(mRequest, POST_ERROR_EVENT_ILLEGAL_TYPE);
          NS_DispatchToMainThread(r);
          return NS_OK;
        }

        if (XRE_GetProcessType() != GeckoProcessType_Default) {
          PDeviceStorageRequestChild* child = new DeviceStorageRequestChild(mRequest, mFile);
          DeviceStorageDeleteParams params(mFile->mStorageType, fullpath);
          ContentChild::GetSingleton()->SendPDeviceStorageRequestConstructor(child, params);
          return NS_OK;
        }
        r = new DeleteFileEvent(mFile, mRequest);
        break;
      }

      case DEVICE_STORAGE_REQUEST_FREE_SPACE:
      {
        if (XRE_GetProcessType() != GeckoProcessType_Default) {
          PDeviceStorageRequestChild* child = new DeviceStorageRequestChild(mRequest, mFile);
          DeviceStorageFreeSpaceParams params(mFile->mStorageType, fullpath);
          ContentChild::GetSingleton()->SendPDeviceStorageRequestConstructor(child, params);
          return NS_OK;
        }
        r = new FreeSpaceFileEvent(mFile, mRequest);
        break;
      }

      case DEVICE_STORAGE_REQUEST_USED_SPACE:
      {
        if (XRE_GetProcessType() != GeckoProcessType_Default) {
          PDeviceStorageRequestChild* child = new DeviceStorageRequestChild(mRequest, mFile);
          DeviceStorageUsedSpaceParams params(mFile->mStorageType, fullpath);
          ContentChild::GetSingleton()->SendPDeviceStorageRequestConstructor(child, params);
          return NS_OK;
        }
        r = new UsedSpaceFileEvent(mFile, mRequest);
        break;
      }

      case DEVICE_STORAGE_REQUEST_AVAILABLE:
      {
        if (XRE_GetProcessType() != GeckoProcessType_Default) {
          PDeviceStorageRequestChild* child = new DeviceStorageRequestChild(mRequest, mFile);
          DeviceStorageAvailableParams params(mFile->mStorageType, fullpath);
          ContentChild::GetSingleton()->SendPDeviceStorageRequestConstructor(child, params);
          return NS_OK;
        }
        r = new PostAvailableResultEvent(mFile->mPath, mRequest);
        NS_DispatchToMainThread(r);
        return NS_OK;
      }

      case DEVICE_STORAGE_REQUEST_WATCH:
      {
        mDeviceStorage->mAllowedToWatchFile = true;
        return NS_OK;
      }
    }

    if (r) {
      nsCOMPtr<nsIEventTarget> target = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
      NS_ASSERTION(target, "Must have stream transport service");
      target->Dispatch(r, NS_DISPATCH_NORMAL);
    }
    return NS_OK;
  }

  bool Recv__delete__(const bool& allow)
  {
    if (allow) {
      Allow();
    }
    else {
      Cancel();
    }
    return true;
  }

  void IPDLRelease()
  {
    Release();
  }

private:
  int32_t mRequestType;
  nsCOMPtr<nsPIDOMWindow> mWindow;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsRefPtr<DeviceStorageFile> mFile;

  nsRefPtr<DOMRequest> mRequest;
  nsCOMPtr<nsIDOMBlob> mBlob;
  nsRefPtr<nsDOMDeviceStorage> mDeviceStorage;
  nsCOMPtr<nsIDOMEventListener> mListener;
};

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DeviceStorageRequest)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContentPermissionRequest)
  NS_INTERFACE_MAP_ENTRY(nsIContentPermissionRequest)
  NS_INTERFACE_MAP_ENTRY(nsIRunnable)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DeviceStorageRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DeviceStorageRequest)

NS_IMPL_CYCLE_COLLECTION_5(DeviceStorageRequest,
                           mRequest,
                           mWindow,
                           mBlob,
                           mDeviceStorage,
                           mListener)


DOMCI_DATA(DeviceStorage, nsDOMDeviceStorage)

NS_INTERFACE_MAP_BEGIN(nsDOMDeviceStorage)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDeviceStorage)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DeviceStorage)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(nsDOMDeviceStorage, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(nsDOMDeviceStorage, nsDOMEventTargetHelper)

nsDOMDeviceStorage::nsDOMDeviceStorage()
  : mIsWatchingFile(false)
  , mAllowedToWatchFile(false)
{ }

nsresult
nsDOMDeviceStorage::Init(nsPIDOMWindow* aWindow, const nsAString &aType, const nsAString &aVolName)
{
  DebugOnly<FileUpdateDispatcher*> observer = FileUpdateDispatcher::GetSingleton();
  NS_ASSERTION(observer, "FileUpdateDispatcher is null");

  NS_ASSERTION(aWindow, "Must have a content dom");

  SetRootDirectoryForType(aType, aVolName);
  if (!mRootDirectory) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  BindToOwner(aWindow);

  // Grab the principal of the document
  nsCOMPtr<nsIDOMDocument> domdoc;
  aWindow->GetDocument(getter_AddRefs(domdoc));
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);
  if (!doc) {
    return NS_ERROR_FAILURE;
  }
  mPrincipal = doc->NodePrincipal();

  // the 'apps' type is special.  We only want this exposed
  // if the caller has the "webapps-manage" permission.
  if (aType.EqualsLiteral("apps")) {
    nsCOMPtr<nsIPermissionManager> permissionManager = do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
    NS_ENSURE_TRUE(permissionManager, NS_ERROR_FAILURE);

    uint32_t permission;
    nsresult rv = permissionManager->TestPermissionFromPrincipal(mPrincipal,
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
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

#ifdef MOZ_WIDGET_GONK
  UnregisterForSDCardChanges(this);
#endif

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  obs->RemoveObserver(this, "file-watcher-update");
}

void
nsDOMDeviceStorage::GetOrderedVolumeNames(const nsAString &aType,
                                          nsTArray<nsString> &aVolumeNames)
{
#ifdef MOZ_WIDGET_GONK
  if (DeviceStorageTypeChecker::IsVolumeBased(aType)) {
    nsCOMPtr<nsIVolumeService> vs = do_GetService(NS_VOLUMESERVICE_CONTRACTID);
    if (vs) {
      vs->GetVolumeNames(aVolumeNames);

      // If the volume sdcard exists, then we want it to be first.

      nsTArray<nsString>::index_type sdcardIndex;
      sdcardIndex = aVolumeNames.IndexOf(NS_LITERAL_STRING("sdcard"));
      if ((sdcardIndex != nsTArray<nsString>::NoIndex)
      &&  (sdcardIndex > 0)) {
        aVolumeNames.RemoveElementAt(sdcardIndex);
        aVolumeNames.InsertElementAt(0, NS_LITERAL_STRING("sdcard"));
      }
    }
  }
#endif
  if (aVolumeNames.Length() == 0) {
    aVolumeNames.AppendElement(NS_LITERAL_STRING(""));
  }
}

void
nsDOMDeviceStorage::CreateDeviceStoragesFor(nsPIDOMWindow* aWin,
                                            const nsAString &aType,
                                            nsTArray<nsRefPtr<nsDOMDeviceStorage> > &aStores)
{
  nsTArray<nsString>  volNames;
  GetOrderedVolumeNames(aType, volNames);

  nsTArray<nsString>::size_type numVolumeNames = volNames.Length();
  for (nsTArray<nsString>::index_type i = 0; i < numVolumeNames; i++) {
    nsresult rv;
    nsRefPtr<nsDOMDeviceStorage> storage = new nsDOMDeviceStorage();
    rv = storage->Init(aWin, aType, volNames[i]);
    if (NS_FAILED(rv)) {
      break;
    }
    aStores.AppendElement(storage);
  }
}

NS_IMETHODIMP
nsDOMDeviceStorage::Add(nsIDOMBlob *aBlob, nsIDOMDOMRequest * *_retval)
{
  if (!aBlob) {
    return NS_OK;
  }

  nsCOMPtr<nsIMIMEService> mimeSvc = do_GetService(NS_MIMESERVICE_CONTRACTID);
  if (!mimeSvc) {
    return NS_ERROR_FAILURE;
  }

  // if mimeType isn't set, we will not get a correct
  // extension, and AddNamed() will fail.  This will post an
  // onerror to the requestee.
  nsString mimeType;
  aBlob->GetType(mimeType);

  nsCString extension;
  mimeSvc->GetPrimaryExtension(NS_LossyConvertUTF16toASCII(mimeType), EmptyCString(), extension);
  // if extension is null here, we will ignore it for now.
  // AddNamed() will check the file path and fail.  This
  // will post an onerror to the requestee.

  // possible race here w/ unique filename
  char buffer[128];
  NS_MakeRandomString(buffer, ArrayLength(buffer));

  nsAutoCString path;
  path.Assign(nsDependentCString(buffer));
  path.Append(".");
  path.Append(extension);

  return AddNamed(aBlob, NS_ConvertASCIItoUTF16(path), _retval);
}

NS_IMETHODIMP
nsDOMDeviceStorage::AddNamed(nsIDOMBlob *aBlob,
                             const nsAString & aPath,
                             nsIDOMDOMRequest * *_retval)
{
  // if the blob is null here, bail
  if (aBlob == nullptr)
    return NS_OK;

  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    return NS_ERROR_UNEXPECTED;
  }

  DeviceStorageTypeChecker* typeChecker = DeviceStorageTypeChecker::CreateOrGet();
  if (!typeChecker) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  NS_ADDREF(*_retval = request);

  nsCOMPtr<nsIRunnable> r;

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType, mRootDirectory, aPath);
  if (!dsf->IsSafePath()) {
    r = new PostErrorEvent(request, POST_ERROR_EVENT_PERMISSION_DENIED);
  } else if (!typeChecker->Check(mStorageType, dsf->mFile) ||
      !typeChecker->Check(mStorageType, aBlob)) {
    r = new PostErrorEvent(request, POST_ERROR_EVENT_ILLEGAL_TYPE);
  }
  else {
    r = new DeviceStorageRequest(DEVICE_STORAGE_REQUEST_CREATE,
                                 win, mPrincipal, dsf, request, aBlob);
  }

  NS_DispatchToMainThread(r);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceStorage::Get(const JS::Value & aPath,
                        JSContext* aCx,
                        nsIDOMDOMRequest * *_retval)
{
  return GetInternal(aPath, aCx, _retval, false);
}

NS_IMETHODIMP
nsDOMDeviceStorage::GetEditable(const JS::Value & aPath,
                                JSContext* aCx,
                                nsIDOMDOMRequest * *_retval)
{
  return GetInternal(aPath, aCx, _retval, true);
}

nsresult
nsDOMDeviceStorage::GetInternal(const JS::Value & aPath,
                                JSContext* aCx,
                                nsIDOMDOMRequest * *_retval,
                                bool aEditable)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    return NS_ERROR_UNEXPECTED;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  NS_ADDREF(*_retval = request);

  nsCOMPtr<nsIRunnable> r;

  JSString* jsstr = JS_ValueToString(aCx, aPath);
  nsDependentJSString path;
  if (!path.init(aCx, jsstr)) {
    r = new PostErrorEvent(request, POST_ERROR_EVENT_UNKNOWN);
    NS_DispatchToMainThread(r);
    return NS_OK;
  }

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType, mRootDirectory, path);
  dsf->SetEditable(aEditable);
  if (!dsf->IsSafePath()) {
    r = new PostErrorEvent(request, POST_ERROR_EVENT_PERMISSION_DENIED);
  } else {
    r = new DeviceStorageRequest(aEditable ? DEVICE_STORAGE_REQUEST_WRITE : DEVICE_STORAGE_REQUEST_READ,
                                 win, mPrincipal, dsf, request);
  }
  NS_DispatchToMainThread(r);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceStorage::Delete(const JS::Value & aPath, JSContext* aCx, nsIDOMDOMRequest * *_retval)
{
  nsCOMPtr<nsIRunnable> r;

  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    return NS_ERROR_UNEXPECTED;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  NS_ADDREF(*_retval = request);

  JSString* jsstr = JS_ValueToString(aCx, aPath);
  nsDependentJSString path;
  if (!path.init(aCx, jsstr)) {
    r = new PostErrorEvent(request, POST_ERROR_EVENT_UNKNOWN);
    NS_DispatchToMainThread(r);
    return NS_OK;
  }

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType, mRootDirectory, path);

  if (!dsf->IsSafePath()) {
    r = new PostErrorEvent(request, POST_ERROR_EVENT_PERMISSION_DENIED);
  }
  else {
    r = new DeviceStorageRequest(DEVICE_STORAGE_REQUEST_DELETE,
                                 win, mPrincipal, dsf, request);
  }
  NS_DispatchToMainThread(r);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceStorage::FreeSpace(nsIDOMDOMRequest** aRetval)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    return NS_ERROR_UNEXPECTED;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  NS_ADDREF(*aRetval = request);

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType, mRootDirectory);
  nsCOMPtr<nsIRunnable> r = new DeviceStorageRequest(DEVICE_STORAGE_REQUEST_FREE_SPACE,
                                                     win,
                                                     mPrincipal,
                                                     dsf,
                                                     request);
  NS_DispatchToMainThread(r);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceStorage::UsedSpace(nsIDOMDOMRequest** aRetval)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    return NS_ERROR_UNEXPECTED;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  NS_ADDREF(*aRetval = request);

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType, mRootDirectory);
  nsCOMPtr<nsIRunnable> r = new DeviceStorageRequest(DEVICE_STORAGE_REQUEST_USED_SPACE,
                                                     win,
                                                     mPrincipal,
                                                     dsf,
                                                     request);
  NS_DispatchToMainThread(r);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceStorage::Available(nsIDOMDOMRequest** aRetval)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    return NS_ERROR_UNEXPECTED;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  NS_ADDREF(*aRetval = request);

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType, mRootDirectory);
  nsCOMPtr<nsIRunnable> r = new DeviceStorageRequest(DEVICE_STORAGE_REQUEST_AVAILABLE,
                                                     win,
                                                     mPrincipal,
                                                     dsf,
                                                     request);
  NS_DispatchToMainThread(r);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceStorage::GetRootDirectory(nsIFile** aRootDirectory)
{
  if (!mRootDirectory) {
    return NS_ERROR_FAILURE;
  }
  return mRootDirectory->Clone(aRootDirectory);
}

NS_IMETHODIMP
nsDOMDeviceStorage::GetVolumeName(nsAString & aVolumeName)
{
  aVolumeName = mVolumeName;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceStorage::Enumerate(const JS::Value & aName,
                             const JS::Value & aOptions,
                             JSContext* aCx,
                             uint8_t aArgc,
                             nsIDOMDOMCursor** aRetval)
{
  return EnumerateInternal(aName, aOptions, aCx, aArgc, false, aRetval);
}

NS_IMETHODIMP
nsDOMDeviceStorage::EnumerateEditable(const JS::Value & aName,
                                     const JS::Value & aOptions,
                                     JSContext* aCx,
                                     uint8_t aArgc,
                                     nsIDOMDOMCursor** aRetval)
{
  return EnumerateInternal(aName, aOptions, aCx, aArgc, true, aRetval);
}


static PRTime
ExtractDateFromOptions(JSContext* aCx, const JS::Value& aOptions)
{
  PRTime result = 0;
  mozilla::idl::DeviceStorageEnumerationParameters params;
  if (!JSVAL_IS_VOID(aOptions) && !aOptions.isNull()) {
    nsresult rv = params.Init(aCx, &aOptions);
    if (NS_SUCCEEDED(rv) && !JSVAL_IS_VOID(params.since) && !params.since.isNull() && params.since.isObject()) {
      JSObject* obj = JSVAL_TO_OBJECT(params.since);
      if (JS_ObjectIsDate(aCx, obj) && js_DateIsValid(obj)) {
        result = js_DateGetMsecSinceEpoch(obj);
      }
    }
  }
  return result;
}

nsresult
nsDOMDeviceStorage::EnumerateInternal(const JS::Value & aName,
                                     const JS::Value & aOptions,
                                     JSContext* aCx,
                                     uint8_t aArgc,
                                     bool aEditable,
                                     nsIDOMDOMCursor** aRetval)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win)
    return NS_ERROR_UNEXPECTED;

  PRTime since = 0;
  nsString path;
  path.SetIsVoid(true);

  if (aArgc > 0) {
    // inspect the first value to see if it is a string
    if (JSVAL_IS_STRING(aName)) {
      JSString* jsstr = JS_ValueToString(aCx, aName);
      nsDependentJSString jspath;
      jspath.init(aCx, jsstr);
      path.Assign(jspath);
    } else if (!JSVAL_IS_PRIMITIVE(aName)) {
      // it also might be an options object
      since = ExtractDateFromOptions(aCx, aName);
    } else {
      return NS_ERROR_FAILURE;
    }

    if (aArgc == 2 && (JSVAL_IS_VOID(aOptions) || aOptions.isNull() || !aOptions.isObject())) {
      return NS_ERROR_FAILURE;
    }
    since = ExtractDateFromOptions(aCx, aOptions);
  }

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType, mRootDirectory, path);
  dsf->SetEditable(aEditable);

  nsRefPtr<nsDOMDeviceStorageCursor> cursor = new nsDOMDeviceStorageCursor(win, mPrincipal,
                                                                           dsf, since);
  nsRefPtr<DeviceStorageCursorRequest> r = new DeviceStorageCursorRequest(cursor);

  NS_ADDREF(*aRetval = cursor);

  if (mozilla::Preferences::GetBool("device.storage.prompt.testing", false)) {
    r->Allow();
    return NS_OK;
  }

  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    // because owner implements nsITabChild, we can assume that it is
    // the one and only TabChild.
    TabChild* child = GetTabChildFrom(win->GetDocShell());
    if (!child)
      return NS_OK;

    // Retain a reference so the object isn't deleted without IPDL's knowledge.
    // Corresponding release occurs in DeallocPContentPermissionRequest.
    r->AddRef();

    nsCString type;
    nsresult rv = DeviceStorageTypeChecker::GetPermissionForType(mStorageType, type);
    if (NS_FAILED(rv)) {
      return rv;
    }
    child->SendPContentPermissionRequestConstructor(r, type, NS_LITERAL_CSTRING("read"), IPC::Principal(mPrincipal));

    r->Sendprompt();

    return NS_OK;
  }

  nsCOMPtr<nsIContentPermissionPrompt> prompt = do_CreateInstance(NS_CONTENT_PERMISSION_PROMPT_CONTRACTID);
  if (prompt) {
    prompt->Prompt(r);
  }

  return NS_OK;
}

#ifdef MOZ_WIDGET_GONK
void
nsDOMDeviceStorage::DispatchMountChangeEvent(nsAString& aType)
{
  nsCOMPtr<nsIDOMEvent> event;
  NS_NewDOMDeviceStorageChangeEvent(getter_AddRefs(event), this, nullptr, nullptr);

  nsCOMPtr<nsIDOMDeviceStorageChangeEvent> ce = do_QueryInterface(event);
  nsresult rv = ce->InitDeviceStorageChangeEvent(NS_LITERAL_STRING("change"),
                                                 true, false,
                                                 NS_LITERAL_STRING(""),
                                                 aType);
  if (NS_FAILED(rv)) {
    return;
  }

  bool ignore;
  DispatchEvent(ce, &ignore);
}
#endif

NS_IMETHODIMP
nsDOMDeviceStorage::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *aData)
{
  if (!strcmp(aTopic, "file-watcher-update")) {

    DeviceStorageFile* file = static_cast<DeviceStorageFile*>(aSubject);
    Notify(NS_ConvertUTF16toUTF8(aData).get(), file);
    return NS_OK;
  }

#ifdef MOZ_WIDGET_GONK
  else if (!strcmp(aTopic, NS_VOLUME_STATE_CHANGED)) {
    nsCOMPtr<nsIVolume> vol = do_QueryInterface(aSubject);
    if (!vol) {
      return NS_OK;
    }
    nsString volName;
    vol->GetName(volName);
    if (!volName.EqualsLiteral("sdcard")) {
      return NS_OK;
    }

    int32_t state;
    nsresult rv = vol->GetState(&state);
    if (NS_FAILED(rv)) {
      return NS_OK;
    }

    nsString type;
    if (state == nsIVolume::STATE_MOUNTED) {
      type.Assign(NS_LITERAL_STRING("available"));
    } else if (state == nsIVolume::STATE_SHARED || state == nsIVolume::STATE_SHAREDMNT) {
      type.Assign(NS_LITERAL_STRING("shared"));
    } else if (state == nsIVolume::STATE_NOMEDIA || state == nsIVolume::STATE_UNMOUNTING) {
      type.Assign(NS_LITERAL_STRING("unavailable"));
    } else {
      // ignore anything else.
      return NS_OK;
    }
    DispatchMountChangeEvent(type);
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

  if (!mStorageType.Equals(aFile->mStorageType)) {
    // Ignore this
    return NS_OK;
  }

  if (!mRootDirectory) {
    return NS_ERROR_FAILURE;
  }

  nsString rootpath;
  nsresult rv = mRootDirectory->GetPath(rootpath);
  if (NS_FAILED(rv)) {
    return NS_OK;
  }

  nsString fullpath;
  rv = aFile->mFile->GetPath(fullpath);
  if (NS_FAILED(rv)) {
    return NS_OK;
  }

  NS_ASSERTION(fullpath.Length() >= rootpath.Length(), "Root path longer than full path!");

  nsAString::size_type len = rootpath.Length() + 1; // +1 for the trailing /
  nsDependentSubstring newPath (fullpath, len, fullpath.Length() - len);

  nsCOMPtr<nsIDOMEvent> event;
  NS_NewDOMDeviceStorageChangeEvent(getter_AddRefs(event), this,
                                    nullptr, nullptr);

  nsCOMPtr<nsIDOMDeviceStorageChangeEvent> ce = do_QueryInterface(event);

  nsString reason;
  reason.AssignWithConversion(aReason);
  rv = ce->InitDeviceStorageChangeEvent(NS_LITERAL_STRING("change"), true, false, newPath, reason);
  NS_ENSURE_SUCCESS(rv, rv);

  bool ignore;
  DispatchEvent(ce, &ignore);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceStorage::AddEventListener(const nsAString & aType,
                                     nsIDOMEventListener *aListener,
                                     bool aUseCapture,
                                     bool aWantsUntrusted,
                                     uint8_t aArgc)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    return NS_ERROR_UNEXPECTED;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType, mRootDirectory);
  nsCOMPtr<nsIRunnable> r = new DeviceStorageRequest(DEVICE_STORAGE_REQUEST_WATCH,
                                                     win, mPrincipal, dsf, request, this, aListener);
  NS_DispatchToMainThread(r);
  return nsDOMEventTargetHelper::AddEventListener(aType, aListener, aUseCapture, aWantsUntrusted, aArgc);
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

  return nsDOMDeviceStorage::AddEventListener(aType,aListener,aUseCapture,aWantsUntrusted, aArgc);
}

NS_IMETHODIMP
nsDOMDeviceStorage::RemoveEventListener(const nsAString & aType,
                                        nsIDOMEventListener *aListener,
                                        bool aUseCapture)
{
  nsDOMEventTargetHelper::RemoveEventListener(aType, aListener, false);

  if (mIsWatchingFile && !HasListenersFor(nsGkAtoms::onchange)) {
    mIsWatchingFile = false;
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    obs->RemoveObserver(this, "file-watcher-update");
  }
  return NS_OK;
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
  return nsDOMEventTargetHelper::DispatchEvent(aEvt, aRetval);
}

EventTarget*
nsDOMDeviceStorage::GetTargetForDOMEvent()
{
  return nsDOMEventTargetHelper::GetTargetForDOMEvent();
}

EventTarget *
nsDOMDeviceStorage::GetTargetForEventTargetChain()
{
  return nsDOMEventTargetHelper::GetTargetForEventTargetChain();
}

nsresult
nsDOMDeviceStorage::PreHandleEvent(nsEventChainPreVisitor & aVisitor)
{
  return nsDOMEventTargetHelper::PreHandleEvent(aVisitor);
}

nsresult
nsDOMDeviceStorage::WillHandleEvent(nsEventChainPostVisitor & aVisitor)
{
  return nsDOMEventTargetHelper::WillHandleEvent(aVisitor);
}

nsresult
nsDOMDeviceStorage::PostHandleEvent(nsEventChainPostVisitor & aVisitor)
{
  return nsDOMEventTargetHelper::PostHandleEvent(aVisitor);
}

nsresult
nsDOMDeviceStorage::DispatchDOMEvent(nsEvent *aEvent,
                                     nsIDOMEvent *aDOMEvent,
                                     nsPresContext *aPresContext,
                                     nsEventStatus *aEventStatus)
{
  return nsDOMEventTargetHelper::DispatchDOMEvent(aEvent,
                                                  aDOMEvent,
                                                  aPresContext,
                                                  aEventStatus);
}

nsEventListenerManager *
nsDOMDeviceStorage::GetListenerManager(bool aMayCreate)
{
  return nsDOMEventTargetHelper::GetListenerManager(aMayCreate);
}

nsIScriptContext *
nsDOMDeviceStorage::GetContextForEventHandlers(nsresult *aRv)
{
  return nsDOMEventTargetHelper::GetContextForEventHandlers(aRv);
}

JSContext *
nsDOMDeviceStorage::GetJSContextForEventHandlers()
{
  return nsDOMEventTargetHelper::GetJSContextForEventHandlers();
}

NS_IMPL_EVENT_HANDLER(nsDOMDeviceStorage, change)
