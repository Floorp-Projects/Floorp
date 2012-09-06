/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/PBrowserChild.h"
#include "mozilla/dom/ipc/Blob.h"
#include "mozilla/dom/devicestorage/PDeviceStorageRequestChild.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/PContentPermissionRequestChild.h"

#include "nsDeviceStorage.h"

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
#include "mozilla/Preferences.h"
#include "nsJSUtils.h"
#include "DictionaryHelpers.h"
#include "mozilla/Attributes.h"
#include "nsContentUtils.h"
#include "nsXULAppAPI.h"
#include "TabChild.h"
#include "DeviceStorageRequestChild.h"
#include "nsIDOMDeviceStorageChangeEvent.h"
#include "nsCRT.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"
#include "GeneratedEvents.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "nsIMIMEService.h"
#include "nsCExternalHandlerService.h"

// Microsoft's API Name hackery sucks
#undef CreateEvent

#ifdef MOZ_WIDGET_GONK
#include "nsIVolume.h"
#include "nsIVolumeService.h"
#endif

#define DEBUG_ISTYPE 1

#ifdef DEBUG_ISTYPE
#include "nsIConsoleService.h"
#endif

using namespace mozilla::dom;
using namespace mozilla::dom::devicestorage;

#include "nsDirectoryServiceDefs.h"

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
    obs->NotifyObservers(mFile, "file-watcher-update", data.get());
    return NS_OK;
  }

private:
  nsRefPtr<DeviceStorageFile> mFile;
  nsCString mType;
};

DeviceStorageFile::DeviceStorageFile(const nsAString& aStorageType, nsIFile* aFile, const nsAString& aPath)
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
}

DeviceStorageFile::DeviceStorageFile(const nsAString& aStorageType, nsIFile* aFile)
  : mStorageType(aStorageType)
  , mEditable(false)
{
  NS_ASSERTION(aFile, "Must not create a DeviceStorageFile with a null nsIFile");
  // always take a clone
  nsCOMPtr<nsIFile> file;
  aFile->Clone(getter_AddRefs(mFile));
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

bool
DeviceStorageFile::IsType(nsAString& aType)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

#ifdef DEBUG_ISTYPE
  nsCOMPtr<nsIConsoleService> svc = do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  char buffer[1024];
  nsCString path;
  mFile->GetNativePath(path);

  PRIntervalTime iStart = PR_IntervalNow();
#endif

  nsAutoCString mimeType;
  nsCOMPtr<nsIMIMEService> mimeService = do_GetService(NS_MIMESERVICE_CONTRACTID);
  if (!mimeService) {
    return false;
  }

  nsresult rv = mimeService->GetTypeFromFile(mFile, mimeType);
  if (NS_FAILED(rv)) {
#ifdef DEBUG_ISTYPE
    sprintf(buffer, "GetTypeFromFile failed for %s (took: %dms)\n",
	    path.get(),
	    PR_IntervalToMilliseconds(PR_IntervalNow() - iStart));

    nsString data;
    CopyASCIItoUTF16(buffer, data);
    svc->LogStringMessage(data.get());
    printf("%s\n", buffer);
#endif
    return false;
  }

#ifdef DEBUG_ISTYPE
  sprintf(buffer, "IsType of %s is %s (took: %dms)\n",
	  path.get(),
	  mimeType.get(),
	  PR_IntervalToMilliseconds(PR_IntervalNow() - iStart));

  nsString data;
  CopyASCIItoUTF16(buffer, data);
  svc->LogStringMessage(data.get());
  printf("%s\n", buffer);
#endif

  if (aType.Equals(NS_LITERAL_STRING("pictures"))) {
    return StringBeginsWith(mimeType, NS_LITERAL_CSTRING("image/"));
  }

  if (aType.Equals(NS_LITERAL_STRING("videos"))) {
    return StringBeginsWith(mimeType, NS_LITERAL_CSTRING("video/"));
  }

  if (aType.Equals(NS_LITERAL_STRING("music"))) {
    return StringBeginsWith(mimeType, NS_LITERAL_CSTRING("audio/"));
  }

  return false;
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
  NS_NewBufferedOutputStream(getter_AddRefs(bufferedOutputStream),
                             outputStream,
                             4096*4);

  if (!bufferedOutputStream) {
    return NS_ERROR_FAILURE;
  }

  rv = NS_OK;
  while (bufSize) {
    uint32_t wrote;
    rv = bufferedOutputStream->WriteFrom(aInputStream,
					 static_cast<uint32_t>(NS_MIN<uint64_t>(bufSize, PR_UINT32_MAX)),
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
                                uint64_t aSince)
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
                                        uint64_t aSince,
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
DeviceStorageFile::DirectoryDiskUsage(nsIFile* aFile, uint64_t* aSoFar)
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
      DirectoryDiskUsage(f, aSoFar);
    } else if (isFile) {
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
GetSDCardStatus(nsAString& aState) {
  nsCOMPtr<nsIVolumeService> vs = do_GetService(NS_VOLUMESERVICE_CONTRACTID);
  if (!vs) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIVolume> vol;
  vs->GetVolumeByName(NS_LITERAL_STRING("sdcard"), getter_AddRefs(vol));
  if (!vol) {
    return NS_ERROR_FAILURE;
  }

  int32_t state;
  nsresult rv = vol->GetState(&state);
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  if (state == nsIVolume::STATE_MOUNTED) {
    aState.AssignASCII("available");
  } else if (state == nsIVolume::STATE_SHARED || state == nsIVolume::STATE_SHAREDMNT) {
    aState.AssignASCII("shared");
  } else {
    aState.AssignASCII("unavailable");
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
nsDOMDeviceStorage::SetRootDirectoryForType(const nsAString& aType)
{
  nsCOMPtr<nsIFile> f;
  nsCOMPtr<nsIProperties> dirService = do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
  NS_ASSERTION(dirService, "Must have directory service");

  // Picture directory
  if (aType.Equals(NS_LITERAL_STRING("pictures"))) {
#ifdef MOZ_WIDGET_GONK
    NS_NewLocalFile(NS_LITERAL_STRING("/sdcard"), false, getter_AddRefs(f));
#elif defined (MOZ_WIDGET_COCOA)
    dirService->Get(NS_OSX_PICTURE_DOCUMENTS_DIR, NS_GET_IID(nsIFile), getter_AddRefs(f));
#elif defined (XP_UNIX)
    dirService->Get(NS_UNIX_XDG_PICTURES_DIR, NS_GET_IID(nsIFile), getter_AddRefs(f));
#elif defined (XP_WIN)
    dirService->Get(NS_WIN_PERSONAL_DIR, NS_GET_IID(nsIFile), getter_AddRefs(f));
#endif
  }

  // Video directory
  else if (aType.Equals(NS_LITERAL_STRING("videos"))) {
#ifdef MOZ_WIDGET_GONK
    NS_NewLocalFile(NS_LITERAL_STRING("/sdcard"), false, getter_AddRefs(f));
#elif defined (MOZ_WIDGET_COCOA)
    dirService->Get(NS_OSX_MOVIE_DOCUMENTS_DIR, NS_GET_IID(nsIFile), getter_AddRefs(f));
#elif defined (XP_UNIX)
    dirService->Get(NS_UNIX_XDG_VIDEOS_DIR, NS_GET_IID(nsIFile), getter_AddRefs(f));
#elif defined (XP_WIN)
    dirService->Get(NS_WIN_PERSONAL_DIR, NS_GET_IID(nsIFile), getter_AddRefs(f));
#endif
  }

  // Music directory
  else if (aType.Equals(NS_LITERAL_STRING("music"))) {
#ifdef MOZ_WIDGET_GONK
    NS_NewLocalFile(NS_LITERAL_STRING("/sdcard"), false, getter_AddRefs(f));
#elif defined (MOZ_WIDGET_COCOA)
    dirService->Get(NS_OSX_MUSIC_DOCUMENTS_DIR, NS_GET_IID(nsIFile), getter_AddRefs(f));
#elif defined (XP_UNIX)
    dirService->Get(NS_UNIX_XDG_MUSIC_DIR, NS_GET_IID(nsIFile), getter_AddRefs(f));
#elif defined (XP_WIN)
    dirService->Get(NS_WIN_PERSONAL_DIR, NS_GET_IID(nsIFile), getter_AddRefs(f));
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

  JSContext *cx = scriptContext->GetNativeContext();
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

  if (aFile->mEditable) {
    // TODO - needs janv's file handle support.
    return JSVAL_NULL;
  }

  if (aFile == nullptr) {
    return JSVAL_NULL;
  }

  nsCOMPtr<nsIDOMBlob> blob = new nsDOMFileFile(aFile->mFile, aFile->mPath);
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

  JSContext *cx = scriptContext->GetNativeContext();
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
NS_IMPL_CYCLE_COLLECTION_CLASS(DeviceStorageCursorRequest)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DeviceStorageCursorRequest)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mCursor)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DeviceStorageCursorRequest)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mCursor, nsIDOMDeviceStorageCursor)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END


class PostErrorEvent : public nsRunnable
{
public:
  PostErrorEvent(nsRefPtr<DOMRequest>& aRequest, const char* aMessage, DeviceStorageFile* aFile)
  {
    mRequest.swap(aRequest);
    BuildErrorString(aMessage, aFile);
  }

  PostErrorEvent(DOMRequest* aRequest, const char* aMessage, DeviceStorageFile* aFile)
    : mRequest(aRequest)
  {
    BuildErrorString(aMessage, aFile);
  }

  ~PostErrorEvent() {}

  void BuildErrorString(const char* aMessage, DeviceStorageFile* aFile)
  {
    nsAutoString fullPath;

    if (aFile && aFile->mFile) {
      aFile->mFile->GetPath(fullPath);
    }
    else {
      fullPath.Assign(NS_LITERAL_STRING("null file"));
    }

    mError = NS_ConvertASCIItoUTF16(aMessage);
    mError.Append(NS_LITERAL_STRING(" file path = "));
    mError.Append(fullPath.get());
    mError.Append(NS_LITERAL_STRING(" path = "));

    if (aFile) {
      mError.Append(aFile->mPath);
    }
    else {
      mError.Append(NS_LITERAL_STRING("null path"));
    }
  }

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

ContinueCursorEvent::~ContinueCursorEvent() {}

NS_IMETHODIMP
ContinueCursorEvent::Run() {
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  jsval val = JSVAL_NULL;

  nsDOMDeviceStorageCursor* cursor = static_cast<nsDOMDeviceStorageCursor*>(mRequest.get());
  nsString cursorStorageType;
  cursor->GetStorageType(cursorStorageType);

  while (cursor->mFiles.Length() > 0) {
    nsRefPtr<DeviceStorageFile> file = cursor->mFiles[0];
    cursor->mFiles.RemoveElementAt(0);
    if (!file->IsType(cursorStorageType)) {
      continue;
    }
    val = nsIFileToJsval(cursor->GetOwner(), file);
    cursor->mOkToCallContinue = true;
    break;
  }

  mRequest->FireSuccess(val);
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
      nsCOMPtr<PostErrorEvent> event = new PostErrorEvent(mRequest,
                                                          POST_ERROR_EVENT_FILE_NOT_ENUMERABLE,
                                                          mFile);
      NS_DispatchToMainThread(event);
      return NS_OK;
    }

    nsDOMDeviceStorageCursor* cursor = static_cast<nsDOMDeviceStorageCursor*>(mRequest.get());
    mFile->CollectFiles(cursor->mFiles, cursor->mSince);

    nsCOMPtr<ContinueCursorEvent> event = new ContinueCursorEvent(mRequest);
    NS_DispatchToMainThread(event);

    return NS_OK;
  }


private:
  nsRefPtr<DeviceStorageFile> mFile;
  nsRefPtr<DOMRequest> mRequest;
};

DOMCI_DATA(DeviceStorageCursor, nsDOMDeviceStorageCursor)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMDeviceStorageCursor)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDeviceStorageCursor)
  NS_INTERFACE_MAP_ENTRY(nsIContentPermissionRequest)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDOMRequest)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMDeviceStorageCursor)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DeviceStorageCursor)
NS_INTERFACE_MAP_END_INHERITING(DOMRequest)

NS_IMPL_ADDREF_INHERITED(nsDOMDeviceStorageCursor, DOMRequest)
NS_IMPL_RELEASE_INHERITED(nsDOMDeviceStorageCursor, DOMRequest)

nsDOMDeviceStorageCursor::nsDOMDeviceStorageCursor(nsIDOMWindow* aWindow,
                                                   nsIPrincipal* aPrincipal,
                                                   DeviceStorageFile* aFile,
                                                   uint64_t aSince)
  : DOMRequest(aWindow)
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
  aType = "device-storage";
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
  nsCOMPtr<PostErrorEvent> event = new PostErrorEvent(this,
                                                      POST_ERROR_EVENT_PERMISSION_DENIED,
                                                      mFile);
  NS_DispatchToMainThread(event);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceStorageCursor::Allow()
{
  if (!mFile->IsSafePath()) {
    nsCOMPtr<nsIRunnable> r = new PostErrorEvent(this,
                                                 POST_ERROR_EVENT_ILLEGAL_FILE_NAME,
                                                 mFile);
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

NS_IMETHODIMP
nsDOMDeviceStorageCursor::Continue()
{
  if (!mOkToCallContinue) {
    return NS_ERROR_UNEXPECTED;
  }

  if (mRooted) {
    // We call onsuccess multiple times. clear the last
    // rooted result.
    NS_DROP_JS_OBJECTS(this, nsDOMDeviceStorageCursor);
    mResult = JSVAL_VOID;
    mDone = false;
    mRooted = false;
  }

  nsCOMPtr<ContinueCursorEvent> event = new ContinueCursorEvent(this);
  NS_DispatchToMainThread(event);

  mOkToCallContinue = false;
  return NS_OK;
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

class PostStatResultEvent : public nsRunnable
{
public:
  PostStatResultEvent(nsRefPtr<DOMRequest>& aRequest, uint64_t aFreeBytes, uint64_t aTotalBytes)
    : mFreeBytes(aFreeBytes)
    , mTotalBytes(aTotalBytes)
    {
      mRequest.swap(aRequest);
    }

  ~PostStatResultEvent() {}

  NS_IMETHOD Run()
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

    nsString state;
    state.Assign(NS_LITERAL_STRING("available"));
#ifdef MOZ_WIDGET_GONK
    nsresult rv = GetSDCardStatus(state);
    if (NS_FAILED(rv)) {
      state.Assign(NS_LITERAL_STRING("unavailable"));
    }
#endif

    nsRefPtr<nsIDOMDeviceStorageStat> domstat = new nsDOMDeviceStorageStat(mFreeBytes, mTotalBytes, state);

    jsval result = InterfaceToJsval(mRequest->GetOwner(),
				    domstat,
				    &NS_GET_IID(nsIDOMDeviceStorageStat));

    mRequest->FireSuccess(result);
    mRequest = nullptr;
    return NS_OK;
  }

private:
  uint64_t mFreeBytes, mTotalBytes;
  nsString mState;
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

  ~PostResultEvent() {}

  NS_IMETHOD Run()
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

    jsval result = JSVAL_NULL;
    if (mFile) {
      result = nsIFileToJsval(mRequest->GetOwner(), mFile);
    } else {
      result = StringToJsval(mRequest->GetOwner(), mPath);
    }

    mRequest->FireSuccess(result);
    mRequest = nullptr;
    return NS_OK;
  }

private:
  nsRefPtr<DeviceStorageFile> mFile;
  nsString mPath;
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

    nsresult rv = mFile->Write(stream);

    if (NS_FAILED(rv)) {
      mFile->mFile->Remove(false);

      nsCOMPtr<PostErrorEvent> event = new PostErrorEvent(mRequest,
                                                          POST_ERROR_EVENT_UNKNOWN,
                                                          mFile);
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
        r = new PostErrorEvent(mRequest, POST_ERROR_EVENT_FILE_DOES_NOT_EXIST, mFile);
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
      r = new PostErrorEvent(mRequest, POST_ERROR_EVENT_FILE_DOES_NOT_EXIST, mFile);
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

class StatFileEvent : public nsRunnable
{
public:
  StatFileEvent(DeviceStorageFile* aFile, nsRefPtr<DOMRequest>& aRequest)
  : mFile(aFile)
    {
      mRequest.swap(aRequest);
    }

  ~StatFileEvent() {}

  NS_IMETHOD Run()
  {
    NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
    nsCOMPtr<nsIRunnable> r;
    uint64_t diskUsage = 0;
    DeviceStorageFile::DirectoryDiskUsage(mFile->mFile, &diskUsage);
    int64_t freeSpace = 0;
    nsresult rv = mFile->mFile->GetDiskSpaceAvailable(&freeSpace);
    if (NS_FAILED(rv)) {
      freeSpace = 0;
    }

    r = new PostStatResultEvent(mRequest, freeSpace, diskUsage);
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

    enum DeviceStorageRequestType {
        DEVICE_STORAGE_REQUEST_READ,
        DEVICE_STORAGE_REQUEST_WRITE,
        DEVICE_STORAGE_REQUEST_DELETE,
        DEVICE_STORAGE_REQUEST_WATCH,
        DEVICE_STORAGE_REQUEST_STAT
    };

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

      nsCString type = NS_LITERAL_CSTRING("device-storage");
      child->SendPContentPermissionRequestConstructor(this, type, IPC::Principal(mPrincipal));

      Sendprompt();
      return NS_OK;
    }

    nsCOMPtr<nsIContentPermissionPrompt> prompt = do_GetService(NS_CONTENT_PERMISSION_PROMPT_CONTRACTID);
    if (prompt) {
      prompt->Prompt(this);
    }
    return NS_OK;
  }

  NS_IMETHOD GetType(nsACString & aType)
  {
    aType = "device-storage";
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
    nsCOMPtr<PostErrorEvent> event = new PostErrorEvent(mRequest,
                                                        POST_ERROR_EVENT_PERMISSION_DENIED,
                                                        mFile);
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
      case DEVICE_STORAGE_REQUEST_WRITE:
      {
        if (!mBlob) {
          return NS_ERROR_FAILURE;
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
      {
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
        if (XRE_GetProcessType() != GeckoProcessType_Default) {
          PDeviceStorageRequestChild* child = new DeviceStorageRequestChild(mRequest, mFile);
          DeviceStorageDeleteParams params(mFile->mStorageType, fullpath);
          ContentChild::GetSingleton()->SendPDeviceStorageRequestConstructor(child, params);
          return NS_OK;
        }
        r = new DeleteFileEvent(mFile, mRequest);
        break;
      }

      case DEVICE_STORAGE_REQUEST_STAT:
      {
        if (XRE_GetProcessType() != GeckoProcessType_Default) {
          PDeviceStorageRequestChild* child = new DeviceStorageRequestChild(mRequest, mFile);
          DeviceStorageStatParams params(mFile->mStorageType, fullpath);
          ContentChild::GetSingleton()->SendPDeviceStorageRequestConstructor(child, params);
	  return NS_OK;
        }
        r = new StatFileEvent(mFile, mRequest);
        break;
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
NS_IMPL_CYCLE_COLLECTION_CLASS(DeviceStorageRequest)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DeviceStorageRequest)
NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mRequest)
NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mWindow)
NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mBlob)
NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDeviceStorage)
NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mListener)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DeviceStorageRequest)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mRequest, nsIDOMDOMRequest)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mWindow, nsPIDOMWindow)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mBlob, nsIDOMBlob)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mDeviceStorage, nsIDOMDeviceStorage)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mListener, nsIDOMEventListener)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMDeviceStorage)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDOMDeviceStorage, nsDOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDOMDeviceStorage, nsDOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

DOMCI_DATA(DeviceStorage, nsDOMDeviceStorage)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMDeviceStorage)
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
nsDOMDeviceStorage::Init(nsPIDOMWindow* aWindow, const nsAString &aType)
{
  NS_ASSERTION(aWindow, "Must have a content dom");

  SetRootDirectoryForType(aType);
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
nsDOMDeviceStorage::CreateDeviceStoragesFor(nsPIDOMWindow* aWin,
                                            const nsAString &aType,
                                            nsDOMDeviceStorage** aStore)
{
  nsRefPtr<nsDOMDeviceStorage> storage = new nsDOMDeviceStorage();
  if (NS_SUCCEEDED(storage->Init(aWin, aType))) {
    NS_ADDREF(*aStore = storage);
  }
}

NS_IMETHODIMP
nsDOMDeviceStorage::Add(nsIDOMBlob *aBlob, nsIDOMDOMRequest * *_retval)
{
  // possible race here w/ unique filename
  char buffer[128];
  NS_MakeRandomString(buffer, 128);

  nsString path;
  path.AssignWithConversion(nsDependentCString(buffer));

  return AddNamed(aBlob, path, _retval);
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

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  NS_ADDREF(*_retval = request);

  nsCOMPtr<nsIRunnable> r;

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType, mRootDirectory, aPath);

  if (!dsf->IsSafePath()) {
    r = new PostErrorEvent(request, POST_ERROR_EVENT_ILLEGAL_FILE_NAME, dsf);
  }
  else {
    r = new DeviceStorageRequest(DeviceStorageRequest::DEVICE_STORAGE_REQUEST_WRITE,
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
    nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType, mRootDirectory);
    r = new PostErrorEvent(request,
                           POST_ERROR_EVENT_NON_STRING_TYPE_UNSUPPORTED,
                           dsf);
    NS_DispatchToMainThread(r);
    return NS_OK;
  }

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType, mRootDirectory, path);
  dsf->SetEditable(aEditable);

  if (!dsf->IsSafePath()) {
    r = new PostErrorEvent(request, POST_ERROR_EVENT_ILLEGAL_FILE_NAME, dsf);
  } else {
    r = new DeviceStorageRequest(DeviceStorageRequest::DEVICE_STORAGE_REQUEST_READ,
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
    nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType, mRootDirectory);
    r = new PostErrorEvent(request, POST_ERROR_EVENT_NON_STRING_TYPE_UNSUPPORTED, dsf);
    NS_DispatchToMainThread(r);
    return NS_OK;
  }

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType, mRootDirectory, path);

  if (!dsf->IsSafePath()) {
    r = new PostErrorEvent(request, POST_ERROR_EVENT_ILLEGAL_FILE_NAME, dsf);
  }
  else {
    r = new DeviceStorageRequest(DeviceStorageRequest::DEVICE_STORAGE_REQUEST_DELETE,
                                 win, mPrincipal, dsf, request);
  }
  NS_DispatchToMainThread(r);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceStorage::Stat(nsIDOMDOMRequest** aRetval)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    return NS_ERROR_UNEXPECTED;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  NS_ADDREF(*aRetval = request);

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mStorageType, mRootDirectory);
  nsCOMPtr<nsIRunnable> r = new DeviceStorageRequest(DeviceStorageRequest::DEVICE_STORAGE_REQUEST_STAT,
						     win,
						     mPrincipal,
						     dsf,
						     request);
  NS_DispatchToMainThread(r);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceStorage::Enumerate(const JS::Value & aName,
                             const JS::Value & aOptions,
                             JSContext* aCx,
                             uint8_t aArgc,
                             nsIDOMDeviceStorageCursor** aRetval)
{
  return EnumerateInternal(aName, aOptions, aCx, aArgc, false, aRetval);
}

NS_IMETHODIMP
nsDOMDeviceStorage::EnumerateEditable(const JS::Value & aName,
                                     const JS::Value & aOptions,
                                     JSContext* aCx,
                                     uint8_t aArgc,
                                     nsIDOMDeviceStorageCursor** aRetval)
{
  return EnumerateInternal(aName, aOptions, aCx, aArgc, true, aRetval);
}


static PRTime
ExtractDateFromOptions(JSContext* aCx, const JS::Value& aOptions)
{
  PRTime result = 0;
  DeviceStorageEnumerationParameters params;
  if (!JSVAL_IS_VOID(aOptions) && !aOptions.isNull()) {
    nsresult rv = params.Init(aCx, &aOptions);
    if (NS_SUCCEEDED(rv) && !JSVAL_IS_VOID(params.since) && !params.since.isNull() && params.since.isObject()) {
      JSObject* obj = JSVAL_TO_OBJECT(params.since);
      if (JS_ObjectIsDate(aCx, obj) && js_DateIsValid(aCx, obj)) {
        result = js_DateGetMsecSinceEpoch(aCx, obj);
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
                                     nsIDOMDeviceStorageCursor** aRetval)
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

    nsCString type = NS_LITERAL_CSTRING("device-storage");
    child->SendPContentPermissionRequestConstructor(r, type, IPC::Principal(mPrincipal));

    r->Sendprompt();

    return NS_OK;
  }

  nsCOMPtr<nsIContentPermissionPrompt> prompt = do_GetService(NS_CONTENT_PERMISSION_PROMPT_CONTRACTID);
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
  NS_NewDOMDeviceStorageChangeEvent(getter_AddRefs(event), nullptr, nullptr);

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

DOMCI_DATA(DeviceStorageStat, nsDOMDeviceStorageStat)

NS_INTERFACE_MAP_BEGIN(nsDOMDeviceStorageStat)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDeviceStorageStat)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DeviceStorageStat)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsDOMDeviceStorageStat)
NS_IMPL_RELEASE(nsDOMDeviceStorageStat)

nsDOMDeviceStorageStat::nsDOMDeviceStorageStat(uint64_t aFreeBytes, uint64_t aTotalBytes, nsAString& aState)
  : mFreeBytes(aFreeBytes)
  , mTotalBytes(aTotalBytes)
  , mState(aState)
{
}

nsDOMDeviceStorageStat::~nsDOMDeviceStorageStat()
{
}

NS_IMETHODIMP
nsDOMDeviceStorageStat::GetTotalBytes(uint64_t *aTotalBytes)
{
  *aTotalBytes = mTotalBytes;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceStorageStat::GetFreeBytes(uint64_t *aFreeBytes)
{
  *aFreeBytes = mFreeBytes;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceStorageStat::GetState(nsAString& aState)
{
  aState.Assign(mState);
  return NS_OK;
}

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
    if (!volName.Equals(NS_LITERAL_STRING("sdcard"))) {
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
  NS_NewDOMDeviceStorageChangeEvent(getter_AddRefs(event), nullptr, nullptr);

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
  nsCOMPtr<nsIRunnable> r = new DeviceStorageRequest(DeviceStorageRequest::DEVICE_STORAGE_REQUEST_WATCH,
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

  if (mIsWatchingFile && !HasListenersFor(NS_LITERAL_STRING("change"))) {
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

nsIDOMEventTarget *
nsDOMDeviceStorage::GetTargetForDOMEvent()
{
  return nsDOMEventTargetHelper::GetTargetForDOMEvent();
}

nsIDOMEventTarget *
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
