/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDeviceStorage.h"
#include "DOMRequest.h"
#include "nsServiceManagerUtils.h"
#include "nsIFile.h"
#include "nsIDirectoryEnumerator.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIDOMFile.h"
#include "nsDOMBlobBuilder.h"
#include "nsNetUtil.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIContentPermissionPrompt.h"
#include "nsIPrincipal.h"
#include "mozilla/Preferences.h"
#include "nsJSUtils.h"
#include "DictionaryHelpers.h"

using namespace mozilla::dom;

#include "nsDirectoryServiceDefs.h"

class DeviceStorageFile : public nsISupports {
public:

  nsCOMPtr<nsIFile> mFile;
  nsString mPath;

  DeviceStorageFile(nsIFile* aFile, const nsAString& aPath)
  : mPath(aPath)
  {
    NS_ASSERTION(aFile, "Must not create a DeviceStorageFile with a null nsIFile");
    // always take a clone
    nsCOMPtr<nsIFile> file;
    aFile->Clone(getter_AddRefs(mFile));

    AppendRelativePath();

    NormalizeFilePath();
  }

  DeviceStorageFile(nsIFile* aFile)
  {
    NS_ASSERTION(aFile, "Must not create a DeviceStorageFile with a null nsIFile");
    // always take a clone
    nsCOMPtr<nsIFile> file;
    aFile->Clone(getter_AddRefs(mFile));
  }

  void
  setPath(const nsAString& aPath) {
    mPath.Assign(aPath);
    NormalizeFilePath();
  }

  NS_DECL_ISUPPORTS

  // we want to make sure that the names of file can't reach
  // outside of the type of storage the user asked for.
  bool
  isSafePath()
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

private:
  void NormalizeFilePath() {
#if defined(XP_WIN)
    PRUnichar* cur = mPath.BeginWriting();
    PRUnichar* end = mPath.EndWriting();
    for (; cur < end; ++cur) {
      if (PRUnichar('\\') == *cur)
	*cur = PRUnichar('/');
    }
#endif
  }

  void AppendRelativePath() {
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

};

NS_IMPL_THREADSAFE_ISUPPORTS0(DeviceStorageFile)


// TODO - eventually, we will want to factor this method
// out into different system specific subclasses (or
// something)
PRInt32
nsDOMDeviceStorage::SetRootFileForType(const nsAString& aType, const PRInt32 aIndex)
{
  PRInt32 typeResult = DEVICE_STORAGE_TYPE_DEFAULT;

  nsCOMPtr<nsIFile> f;
  nsCOMPtr<nsIProperties> dirService = do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
  NS_ASSERTION(dirService, "Must have directory service");

  // Picture directory
  if (aType.Equals(NS_LITERAL_STRING("pictures"))) {
#ifdef MOZ_WIDGET_GONK
    if (aIndex == 0) {
      NS_NewLocalFile(NS_LITERAL_STRING("/data/pictures"), false, getter_AddRefs(f));
    }
    else if (aIndex == 1) {
      NS_NewLocalFile(NS_LITERAL_STRING("/sdcard/DCIM"), false, getter_AddRefs(f));
      typeResult = DEVICE_STORAGE_TYPE_EXTERNAL;
    }
#elif defined (MOZ_WIDGET_COCOA)
    if (aIndex == 0) {
      dirService->Get(NS_OSX_PICTURE_DOCUMENTS_DIR, NS_GET_IID(nsIFile), getter_AddRefs(f));
    }
#elif defined (XP_UNIX)
    if (aIndex == 0) {
      dirService->Get(NS_UNIX_XDG_PICTURES_DIR, NS_GET_IID(nsIFile), getter_AddRefs(f));
    }
#endif
  }

  // Video directory
  if (aType.Equals(NS_LITERAL_STRING("videos"))) {
#ifdef MOZ_WIDGET_GONK
    if (aIndex == 0) {
      NS_NewLocalFile(NS_LITERAL_STRING("/data/videos"), false, getter_AddRefs(f));
    }
    else if (aIndex == 1) {
      NS_NewLocalFile(NS_LITERAL_STRING("/sdcard/videos"), false, getter_AddRefs(f));
      typeResult = DEVICE_STORAGE_TYPE_EXTERNAL;
    }
#elif defined (MOZ_WIDGET_COCOA)
    if (aIndex == 0) {
      dirService->Get(NS_OSX_MOVIE_DOCUMENTS_DIR, NS_GET_IID(nsIFile), getter_AddRefs(f));
    }
#elif defined (XP_UNIX)
    if (aIndex == 0) {
      dirService->Get(NS_UNIX_XDG_VIDEOS_DIR, NS_GET_IID(nsIFile), getter_AddRefs(f));
    }
#endif
  }

  // Music directory
  if (aType.Equals(NS_LITERAL_STRING("music"))) {
#ifdef MOZ_WIDGET_GONK
    if (aIndex == 0) {
      NS_NewLocalFile(NS_LITERAL_STRING("/data/music"), false, getter_AddRefs(f));
    }
    else if (aIndex == 1) {
      NS_NewLocalFile(NS_LITERAL_STRING("/sdcard/music"), false, getter_AddRefs(f));
      typeResult = DEVICE_STORAGE_TYPE_EXTERNAL;
    }
#elif defined (MOZ_WIDGET_COCOA)
    if (aIndex == 0) {
      dirService->Get(NS_OSX_MUSIC_DOCUMENTS_DIR, NS_GET_IID(nsIFile), getter_AddRefs(f));
    }
#elif defined (XP_UNIX)
    if (aIndex == 0) {
      dirService->Get(NS_UNIX_XDG_MUSIC_DIR, NS_GET_IID(nsIFile), getter_AddRefs(f));
    }
#endif
  }

  // in testing, we have access to a few more directory locations
  if (mozilla::Preferences::GetBool("device.storage.testing", false)) {

    // Temp directory
    if (aType.Equals(NS_LITERAL_STRING("temp")) && aIndex == 0) {
      dirService->Get(NS_OS_TEMP_DIR, NS_GET_IID(nsIFile), getter_AddRefs(f));
    }

    // Profile directory
    else if (aType.Equals(NS_LITERAL_STRING("profile")) && aIndex == 0) {
      dirService->Get(NS_APP_USER_PROFILE_50_DIR, NS_GET_IID(nsIFile), getter_AddRefs(f));
    }
  } 

  mFile = f;
  return typeResult;
}

static jsval nsIFileToJsval(nsPIDOMWindow* aWindow, DeviceStorageFile* aFile, bool aEditable)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aWindow, "Null Window");

  if (aEditable) {
    // TODO - needs janv's file handle support.
    return JSVAL_NULL;
  }

  if (aFile == nsnull) {
    return JSVAL_NULL;
  }

  nsCOMPtr<nsIDOMBlob> blob = new nsDOMFileFile(aFile->mFile, aFile->mPath);

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

  jsval wrappedFile;
  nsresult rv = nsContentUtils::WrapNative(cx,
                                           JS_GetGlobalObject(cx),
                                           blob,
                                           &NS_GET_IID(nsIDOMFile),
                                           &wrappedFile);
  if (NS_FAILED(rv)) {
    return JSVAL_NULL;
  }

  return wrappedFile;
}


static jsval StringToJsval(nsPIDOMWindow* aWindow, nsAString& aString)
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


class nsDOMDeviceStorageCursor
  : public nsIDOMDeviceStorageCursor
  , public DOMRequest
  , public nsIContentPermissionRequest
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSICONTENTPERMISSIONREQUEST
  NS_DECL_NSIDOMDEVICESTORAGECURSOR

  nsDOMDeviceStorageCursor(nsIDOMWindow* aWindow,
                           nsIURI* aURI,
                           DeviceStorageFile* aFile,
                           bool aEditable,
                           PRUint64 aSince);

private:
  ~nsDOMDeviceStorageCursor();

protected:
  nsTArray<nsRefPtr<DeviceStorageFile> > mFiles;

  bool mOkToCallContinue;
  nsRefPtr<DeviceStorageFile> mFile;
  nsCOMPtr<nsIURI> mURI;
  bool mEditable;
  PRUint64 mSince;

  // to access mFiles
  friend class InitCursorEvent;
  friend class ContinueCursorEvent;
};

class DeviceStorageCursorRequest : public nsIContentPermissionRequest
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(DeviceStorageCursorRequest, nsIContentPermissionRequest)

  NS_FORWARD_NSICONTENTPERMISSIONREQUEST(mCursor->);

  DeviceStorageCursorRequest(nsDOMDeviceStorageCursor* aCursor)
    : mCursor(aCursor) { }

  ~DeviceStorageCursorRequest() {}

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


#define POST_ERROR_EVENT_FILE_DOES_NOT_EXIST         "File location doesn't exists"
#define POST_ERROR_EVENT_FILE_NOT_ENUMERABLE         "File location is not enumerable"
#define POST_ERROR_EVENT_PERMISSION_DENIED           "Permission Denied"
#define POST_ERROR_EVENT_ILLEGAL_FILE_NAME           "Illegal file name"
#define POST_ERROR_EVENT_UNKNOWN                     "Unknown"
#define POST_ERROR_EVENT_NON_STRING_TYPE_UNSUPPORTED "Non-string type unsupported"

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

  NS_IMETHOD Run() {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

    mRequest->FireError(mError);
    mRequest = nsnull;
    return NS_OK;
  }

private:
  nsRefPtr<DOMRequest> mRequest;
  nsString mError;
};

class ContinueCursorEvent : public nsRunnable
{
public:

  ContinueCursorEvent(nsRefPtr<DOMRequest>& aRequest)
  {
    mRequest.swap(aRequest);
  }

  ContinueCursorEvent(DOMRequest* aRequest)
    : mRequest(aRequest)
  {
  }

  ~ContinueCursorEvent() {}

  NS_IMETHOD Run() {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

    jsval val;

    nsDOMDeviceStorageCursor* cursor = static_cast<nsDOMDeviceStorageCursor*>(mRequest.get());
    if (cursor->mFiles.Length() == 0) {
      val = JSVAL_NULL;
    }
    else {
      nsRefPtr<DeviceStorageFile> file = cursor->mFiles[0];
      cursor->mFiles.RemoveElementAt(0);
      val = nsIFileToJsval(cursor->GetOwner(), file, cursor->mEditable);
      cursor->mOkToCallContinue = true;
    }

    mRequest->FireSuccess(val);
    mRequest = nsnull;
    return NS_OK;
  }

private:
  nsRefPtr<DOMRequest> mRequest;
};


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

    collectFiles(mFile);

    nsCOMPtr<ContinueCursorEvent> event = new ContinueCursorEvent(mRequest);
    NS_DispatchToMainThread(event);

    return NS_OK;
  }

  void collectFiles(DeviceStorageFile* aFile)
  {
      // TODO - we may want to do this incrementally.
    if (!aFile) {
      return;
    }

    nsCOMPtr<nsISimpleEnumerator> e;
    aFile->mFile->GetDirectoryEntries(getter_AddRefs(e));

    if (!e) {
      return;
    }

    nsCOMPtr<nsIDirectoryEnumerator> files = do_QueryInterface(e);
    nsCOMPtr<nsIFile> f;

    while (NS_SUCCEEDED(files->GetNextFile(getter_AddRefs(f))) && f) {
      nsDOMDeviceStorageCursor* cursor = static_cast<nsDOMDeviceStorageCursor*>(mRequest.get());

      PRInt64 msecs;
      f->GetLastModifiedTime(&msecs);

      if (msecs < (PRInt64) cursor->mSince) {
        continue;
      }

      bool isDir;
      f->IsDirectory(&isDir);

      bool isFile;
      f->IsFile(&isFile);

      nsString fullpath;
      f->GetPath(fullpath);

      nsString rootPath;
      mFile->mFile->GetPath(rootPath);

      if (!StringBeginsWith(fullpath, rootPath)) {
	NS_WARNING("collectFiles returned a path that does not belong!");
	continue;
      }

      nsAString::size_type len = rootPath.Length() + 1; // +1 for the trailing /
      nsDependentSubstring newPath = Substring(fullpath, len);
      nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(f);
      dsf->setPath(newPath);

      if (isDir) {
        collectFiles(dsf);
      }
      else if (isFile) {
        cursor->mFiles.AppendElement(dsf);
      }
    }
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
                                                   nsIURI* aURI,
                                                   DeviceStorageFile* aFile,
                                                   bool aEditable,
                                                   PRUint64 aSince)
  : DOMRequest(aWindow)
  , mOkToCallContinue(false)
  , mFile(aFile)
  , mURI(aURI)
  , mEditable(aEditable)
  , mSince(aSince)
{
}

nsDOMDeviceStorageCursor::~nsDOMDeviceStorageCursor()
{
}

NS_IMETHODIMP
nsDOMDeviceStorageCursor::GetType(nsACString & aType)
{
  aType = "device-storage";
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceStorageCursor::GetUri(nsIURI * *aRequestingURI)
{
  NS_IF_ADDREF(*aRequestingURI = mURI);
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
  *aRequestingElement = nsnull;
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
  if (!mFile->isSafePath()) {
    nsCOMPtr<nsIRunnable> r = new PostErrorEvent(this,
                                                 POST_ERROR_EVENT_ILLEGAL_FILE_NAME,
                                                 mFile);
    NS_DispatchToMainThread(r);
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


class PostResultEvent : public nsRunnable
{
public:
  PostResultEvent(nsRefPtr<DOMRequest>& aRequest, bool aEditable, DeviceStorageFile* aFile)
    : mEditable(aEditable)
    , mFile(aFile)
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
      result = nsIFileToJsval(mRequest->GetOwner(), mFile, mEditable);
    } else {
      result = StringToJsval(mRequest->GetOwner(), mPath);
    }

    mRequest->FireSuccess(result);
    mRequest = nsnull;
    return NS_OK;
  }

private:
  bool mEditable;
  nsRefPtr<DeviceStorageFile> mFile;
  nsString mPath;
  nsRefPtr<DOMRequest> mRequest;
};

class WriteFileEvent : public nsRunnable
{
public:
  WriteFileEvent(nsIDOMBlob *aBlob,
                 DeviceStorageFile *aFile,
                 nsRefPtr<DOMRequest>& aRequest)
  : mBlob(aBlob)
  , mFile(aFile)
    {
      mRequest.swap(aRequest);
    }

  ~WriteFileEvent() {}

  void CleanupOnFail(const char* error)
  {
    if (mFile) {
      mFile->mFile->Remove(false);
    }

    nsCOMPtr<PostErrorEvent> event = new PostErrorEvent(mRequest,
                                                        error,
                                                        mFile);
    NS_DispatchToMainThread(event);
  }

  NS_IMETHOD Run() 
  {
    NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

    //TODO - this might be faster if we check to see if
    //these are backed by OS-files, and if so, then just do
    //a copy()

    nsCOMPtr<nsIFile> f = mFile->mFile;

    // This also creates all ancestors
    nsresult rv = f->Create(nsIFile::NORMAL_FILE_TYPE, 00600);
    if (NS_FAILED(rv)) {
      CleanupOnFail(POST_ERROR_EVENT_UNKNOWN " 1 ");
      return NS_OK;
    }
    
    nsCOMPtr<nsIInputStream> stream;
    mBlob->GetInternalStream(getter_AddRefs(stream));

    if (!stream) {
      CleanupOnFail(POST_ERROR_EVENT_UNKNOWN " 2 ");
      return NS_OK;
    }

    PRUint32 bufSize;
    stream->Available(&bufSize);

    nsCOMPtr<nsIOutputStream> outputStream;
    NS_NewLocalFileOutputStream(getter_AddRefs(outputStream), f);

    if (!outputStream) {
      CleanupOnFail(POST_ERROR_EVENT_UNKNOWN " 3 ");
      return NS_OK;
    }

    nsCOMPtr<nsIOutputStream> bufferedOutputStream;
    NS_NewBufferedOutputStream(getter_AddRefs(bufferedOutputStream),
                               outputStream,
                               4096*4);

    if (!bufferedOutputStream) {
      CleanupOnFail(POST_ERROR_EVENT_UNKNOWN " 4" );
      return NS_OK;
    }

    PRUint32 wrote;
    bufferedOutputStream->WriteFrom(stream, bufSize, &wrote);
    bufferedOutputStream->Close();
    outputStream->Close();

    if (bufSize != wrote) {
      CleanupOnFail(POST_ERROR_EVENT_UNKNOWN " 5 " );
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
                  bool aEditable,
                  nsRefPtr<DOMRequest>& aRequest)
  : mFile(aFile)
  , mEditable(aEditable)
    {
      mRequest.swap(aRequest);
    }

  ~ReadFileEvent() {}

  NS_IMETHOD Run() 
  {
    NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

    nsRefPtr<nsRunnable> r;

    if (!mEditable) {
      bool check = false;
      mFile->mFile->Exists(&check);
      if (!check) {
        r = new PostErrorEvent(mRequest, POST_ERROR_EVENT_FILE_DOES_NOT_EXIST, mFile);
      }
    }

    if (!r) {
      r = new PostResultEvent(mRequest, mEditable, mFile);
    }
    NS_DispatchToMainThread(r);
    return NS_OK;
  }

private:
  nsRefPtr<DeviceStorageFile> mFile;
  bool mEditable;
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

    mFile->mFile->Remove(true);

    nsRefPtr<nsRunnable> r;

    bool check = false;
    mFile->mFile->Exists(&check);
    if (check) {
      r = new PostErrorEvent(mRequest, POST_ERROR_EVENT_UNKNOWN, mFile);
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

class DeviceStorageRequest : public nsIContentPermissionRequest, public nsIRunnable
{
public:

    enum {
        DEVICE_STORAGE_REQUEST_READ,
        DEVICE_STORAGE_REQUEST_WRITE,
        DEVICE_STORAGE_REQUEST_DELETE
    };
    DeviceStorageRequest(const PRInt32 aRequestType,
                         nsPIDOMWindow *aWindow,
                         nsIURI *aURI,
                         DeviceStorageFile *aFile,
                         DOMRequest* aRequest,
                         bool aEditable,
                         nsIDOMBlob *aBlob = nsnull)
        : mRequestType(aRequestType)
        , mWindow(aWindow)
        , mURI(aURI)
        , mFile(aFile)
        , mRequest(aRequest)
        , mEditable(aEditable)
        , mBlob(aBlob) {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(DeviceStorageRequest, nsIContentPermissionRequest)

  NS_IMETHOD Run() {

    if (mozilla::Preferences::GetBool("device.storage.prompt.testing", false)) {
      Allow();
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

  NS_IMETHOD GetUri(nsIURI * *aRequestingURI)
  {
    NS_ADDREF(*aRequestingURI = mURI);
    return NS_OK;
  }

  NS_IMETHOD GetWindow(nsIDOMWindow * *aRequestingWindow)
  {
    NS_IF_ADDREF(*aRequestingWindow = mWindow);
    return NS_OK;
  }

  NS_IMETHOD GetElement(nsIDOMElement * *aRequestingElement)
  {
    *aRequestingElement = nsnull;
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

    switch(mRequestType) {
      case DEVICE_STORAGE_REQUEST_WRITE:
      {
        if (!mBlob) {
          return NS_ERROR_FAILURE;
        }

        r = new WriteFileEvent(mBlob, mFile, mRequest);
        break;
      }
      case DEVICE_STORAGE_REQUEST_READ:
      {
        r = new ReadFileEvent(mFile, mEditable, mRequest);
        break;
      }
      case DEVICE_STORAGE_REQUEST_DELETE:
      {
        r = new DeleteFileEvent(mFile, mRequest);
        break;
      }
    }
    
    if (r) {
      nsCOMPtr<nsIEventTarget> target = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
      NS_ASSERTION(target, "Must have stream transport service");
      target->Dispatch(r, NS_DISPATCH_NORMAL);
    }
    return NS_OK;
  }

private:
  PRInt32 mRequestType;
  nsCOMPtr<nsPIDOMWindow> mWindow;
  nsCOMPtr<nsIURI> mURI;
  nsRefPtr<DeviceStorageFile> mFile;

  nsRefPtr<DOMRequest> mRequest;
  bool mEditable;
  nsCOMPtr<nsIDOMBlob> mBlob;
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
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DeviceStorageRequest)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mRequest, nsIDOMDOMRequest)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mWindow, nsPIDOMWindow)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mBlob, nsIDOMBlob)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

DOMCI_DATA(DeviceStorage, nsDOMDeviceStorage)

NS_INTERFACE_MAP_BEGIN(nsDOMDeviceStorage)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDeviceStorage)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMDeviceStorage)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DeviceStorage)
NS_INTERFACE_MAP_END

NS_IMPL_THREADSAFE_ADDREF(nsDOMDeviceStorage)
NS_IMPL_THREADSAFE_RELEASE(nsDOMDeviceStorage)

nsDOMDeviceStorage::nsDOMDeviceStorage()
 : mStorageType(DEVICE_STORAGE_TYPE_DEFAULT)
{
}

nsresult
nsDOMDeviceStorage::Init(nsPIDOMWindow* aWindow, const nsAString &aType, const PRInt32 aIndex)
{
  NS_ASSERTION(aWindow, "Must have a content dom");

  mStorageType = SetRootFileForType(aType, aIndex);
  if (!mFile) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mOwner = do_GetWeakReference(aWindow);
  if (!mOwner) {
    return NS_ERROR_FAILURE;
  }

  // Grab the uri of the document
  nsCOMPtr<nsIDOMDocument> domdoc;
  aWindow->GetDocument(getter_AddRefs(domdoc));
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);
  if (!doc) {
    return NS_ERROR_FAILURE;
  }
  doc->NodePrincipal()->GetURI(getter_AddRefs(mURI));

  return NS_OK;
}

nsDOMDeviceStorage::~nsDOMDeviceStorage()
{
}

void
nsDOMDeviceStorage::CreateDeviceStoragesFor(nsPIDOMWindow* aWin,
                                            const nsAString &aType,
                                            nsIVariant** _retval)
{
  nsTArray<nsRefPtr<nsIDOMDeviceStorage> > stores;

  PRInt32 index = 0;
  while (1) {
    nsresult rv;
    nsRefPtr<nsDOMDeviceStorage> storage = new nsDOMDeviceStorage();
    rv = storage->Init(aWin, aType, index++);
    if (NS_FAILED(rv))
      break;
    stores.AppendElement(storage);
  }

  nsCOMPtr<nsIWritableVariant> result = do_CreateInstance("@mozilla.org/variant;1");
  if (!result) {
    return;
  }

  result->SetAsArray(nsIDataType::VTYPE_INTERFACE,
                     &NS_GET_IID(nsIDOMDeviceStorage),
                     stores.Length(),
                     const_cast<void*>(static_cast<const void*>(stores.Elements())));
  NS_ADDREF(*_retval = result);
}

NS_IMETHODIMP
nsDOMDeviceStorage::GetType(nsAString & aType)
{
  switch(mStorageType) {
    case DEVICE_STORAGE_TYPE_EXTERNAL:
      aType.AssignLiteral("external");
      break;
    case DEVICE_STORAGE_TYPE_SHARED:
      aType.AssignLiteral("shared");
      break;
    case DEVICE_STORAGE_TYPE_DEFAULT:
    default:
      aType.AssignLiteral("default");
      break;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceStorage::Add(nsIDOMBlob *aBlob, nsIDOMDOMRequest * *_retval NS_OUTPARAM)
{
  char buffer[128];
  NS_MakeRandomString(buffer, 128);

  nsString path;
  path.AssignWithConversion(nsDependentCString(buffer));

  return AddNamed(aBlob, path, _retval);
}

NS_IMETHODIMP
nsDOMDeviceStorage::AddNamed(nsIDOMBlob *aBlob,
                             const nsAString & aPath,
                             nsIDOMDOMRequest * *_retval NS_OUTPARAM)
{
  // if the blob is null here, bail
  if (aBlob == nsnull)
    return NS_OK;

  nsCOMPtr<nsPIDOMWindow> win = do_QueryReferent(mOwner);
  if (!win) {
    return NS_ERROR_UNEXPECTED;
  }
  
  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  NS_ADDREF(*_retval = request);

  nsCOMPtr<nsIRunnable> r;

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mFile, aPath);

  if (!dsf->isSafePath()) {
    r = new PostErrorEvent(request, POST_ERROR_EVENT_ILLEGAL_FILE_NAME, dsf);
  }
  else {
    r = new DeviceStorageRequest(DeviceStorageRequest::DEVICE_STORAGE_REQUEST_WRITE,
                                 win, mURI, dsf, request, true, aBlob);
  }
  NS_DispatchToMainThread(r);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceStorage::Get(const JS::Value & aPath,
                        JSContext* aCx,
                        nsIDOMDOMRequest * *_retval NS_OUTPARAM)
{
  return GetInternal(aPath, aCx, _retval, false);
}

NS_IMETHODIMP
nsDOMDeviceStorage::GetEditable(const JS::Value & aPath,
                                JSContext* aCx,
                                nsIDOMDOMRequest * *_retval NS_OUTPARAM)
{
  return GetInternal(aPath, aCx, _retval, true);
}

nsresult
nsDOMDeviceStorage::GetInternal(const JS::Value & aPath,
                                JSContext* aCx,
                                nsIDOMDOMRequest * *_retval NS_OUTPARAM,
                                bool aEditable)
{
  nsCOMPtr<nsPIDOMWindow> win = do_QueryReferent(mOwner);
  if (!win) {
    return NS_ERROR_UNEXPECTED;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  NS_ADDREF(*_retval = request);

  nsCOMPtr<nsIRunnable> r;

  JSString* jsstr = JS_ValueToString(aCx, aPath);
  nsDependentJSString path;
  if (!path.init(aCx, jsstr)) {
    nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mFile);
    r = new PostErrorEvent(request,
                           POST_ERROR_EVENT_NON_STRING_TYPE_UNSUPPORTED,
                           dsf);
    NS_DispatchToMainThread(r);
    return NS_OK;
  }

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mFile, path);

  if (!dsf->isSafePath()) {
    r = new PostErrorEvent(request, POST_ERROR_EVENT_ILLEGAL_FILE_NAME, dsf);
  } else {
    r = new DeviceStorageRequest(DeviceStorageRequest::DEVICE_STORAGE_REQUEST_READ,
                                 win, mURI, dsf, request, aEditable);
  }
  NS_DispatchToMainThread(r);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceStorage::Delete(const JS::Value & aPath, JSContext* aCx, nsIDOMDOMRequest * *_retval NS_OUTPARAM)
{
  nsCOMPtr<nsIRunnable> r;

  nsCOMPtr<nsPIDOMWindow> win = do_QueryReferent(mOwner);
  if (!win) {
    return NS_ERROR_UNEXPECTED;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(win);
  NS_ADDREF(*_retval = request);

  JSString* jsstr = JS_ValueToString(aCx, aPath);
  nsDependentJSString path;
  if (!path.init(aCx, jsstr)) {
    nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mFile);
    r = new PostErrorEvent(request, POST_ERROR_EVENT_NON_STRING_TYPE_UNSUPPORTED, dsf);
    NS_DispatchToMainThread(r);
    return NS_OK;
  }

  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mFile, path);

  if (!dsf->isSafePath()) {
    r = new PostErrorEvent(request, POST_ERROR_EVENT_ILLEGAL_FILE_NAME, dsf);
  }
  else {
    r = new DeviceStorageRequest(DeviceStorageRequest::DEVICE_STORAGE_REQUEST_DELETE,
                                 win, mURI, dsf, request, true);
  }
  NS_DispatchToMainThread(r);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDeviceStorage::Enumerate(const JS::Value & aName,
                             const JS::Value & aOptions,
                             JSContext* aCx,
                             PRUint8 aArgc,
                             nsIDOMDeviceStorageCursor** aRetval)
{
  return EnumerateInternal(aName, aOptions, aCx, aArgc, false, aRetval);
}

NS_IMETHODIMP
nsDOMDeviceStorage::EnumerateEditable(const JS::Value & aName,
                                     const JS::Value & aOptions,
                                     JSContext* aCx,
                                     PRUint8 aArgc,
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
                                     PRUint8 aArgc,
                                     bool aEditable,
                                     nsIDOMDeviceStorageCursor** aRetval)
{
  nsCOMPtr<nsPIDOMWindow> win = do_QueryReferent(mOwner);
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
  
  nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(mFile, path);
  nsRefPtr<nsDOMDeviceStorageCursor> cursor = new nsDOMDeviceStorageCursor(win, mURI, dsf, aEditable, since);
  nsRefPtr<DeviceStorageCursorRequest> r = new DeviceStorageCursorRequest(cursor);

  NS_ADDREF(*aRetval = cursor);

  if (mozilla::Preferences::GetBool("device.storage.prompt.testing", false)) {
    r->Allow();
    return NS_OK;
  }

  nsCOMPtr<nsIContentPermissionPrompt> prompt = do_GetService(NS_CONTENT_PERMISSION_PROMPT_CONTRACTID);
  if (prompt) {
    prompt->Prompt(r);
  }

  return NS_OK;
}
