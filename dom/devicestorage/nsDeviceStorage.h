/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDeviceStorage_h
#define nsDeviceStorage_h

class nsPIDOMWindow;
#include "PCOMContentPermissionRequestChild.h"

#include "DOMRequest.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDOMClassInfoID.h"
#include "nsIClassInfo.h"
#include "nsIContentPermissionPrompt.h"
#include "nsIDOMDeviceStorageCursor.h"
#include "nsIDOMDeviceStorageStat.h"
#include "nsIDOMWindow.h"
#include "nsIURI.h"
#include "nsInterfaceHashtable.h"
#include "nsIPrincipal.h"
#include "nsString.h"
#include "nsWeakPtr.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventTarget.h"
#include "nsIObserver.h"
#include "mozilla/Mutex.h"
#include "DeviceStorage.h"


#define POST_ERROR_EVENT_FILE_DOES_NOT_EXIST         "File location doesn't exists"
#define POST_ERROR_EVENT_FILE_NOT_ENUMERABLE         "File location is not enumerable"
#define POST_ERROR_EVENT_PERMISSION_DENIED           "Permission Denied"
#define POST_ERROR_EVENT_ILLEGAL_FILE_NAME           "Illegal file name"
#define POST_ERROR_EVENT_UNKNOWN                     "Unknown"
#define POST_ERROR_EVENT_NON_STRING_TYPE_UNSUPPORTED "Non-string type unsupported"
#define POST_ERROR_EVENT_NOT_IMPLEMENTED             "Not implemented"

using namespace mozilla::dom;

class DeviceStorageFile MOZ_FINAL
  : public nsISupports {
public:
  nsCOMPtr<nsIFile> mFile;
  nsString mPath;
  bool mEditable;

  DeviceStorageFile(nsIFile* aFile, const nsAString& aPath);
  DeviceStorageFile(nsIFile* aFile);
  void SetPath(const nsAString& aPath);
  void SetEditable(bool aEditable);

  NS_DECL_ISUPPORTS

  // we want to make sure that the names of file can't reach
  // outside of the type of storage the user asked for.
  bool IsSafePath();

  nsresult Remove();
  nsresult Write(nsIInputStream* aInputStream);
  nsresult Write(InfallibleTArray<PRUint8>& bits);
  void CollectFiles(nsTArray<nsRefPtr<DeviceStorageFile> > &aFiles, PRUint64 aSince = 0);
  void collectFilesInternal(nsTArray<nsRefPtr<DeviceStorageFile> > &aFiles, PRUint64 aSince, nsAString& aRootPath);

  static PRUint64 DirectoryDiskUsage(nsIFile* aFile, PRUint64 aSoFar = 0);

private:
  void NormalizeFilePath();
  void AppendRelativePath();
};

class ContinueCursorEvent MOZ_FINAL: public nsRunnable
{
public:
  ContinueCursorEvent(nsRefPtr<DOMRequest>& aRequest);
  ContinueCursorEvent(DOMRequest* aRequest);
  ~ContinueCursorEvent();
  NS_IMETHOD Run();
private:
  nsRefPtr<DOMRequest> mRequest;
};

class nsDOMDeviceStorageCursor MOZ_FINAL
  : public nsIDOMDeviceStorageCursor
  , public DOMRequest
  , public nsIContentPermissionRequest
  , public PCOMContentPermissionRequestChild
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSICONTENTPERMISSIONREQUEST
  NS_DECL_NSIDOMDEVICESTORAGECURSOR

  nsDOMDeviceStorageCursor(nsIDOMWindow* aWindow,
                           nsIPrincipal* aPrincipal,
                           DeviceStorageFile* aFile,
                           PRUint64 aSince);


  nsTArray<nsRefPtr<DeviceStorageFile> > mFiles;
  bool mOkToCallContinue;
  PRUint64 mSince;

  virtual bool Recv__delete__(const bool& allow);
  virtual void IPDLRelease();

private:
  ~nsDOMDeviceStorageCursor();

  nsRefPtr<DeviceStorageFile> mFile;
  nsCOMPtr<nsIPrincipal> mPrincipal;
};

class nsDOMDeviceStorageStat MOZ_FINAL
  : public nsIDOMDeviceStorageStat
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMDEVICESTORAGESTAT

  nsDOMDeviceStorageStat(PRUint64 aFreeBytes, PRUint64 aTotalBytes, nsAString& aState);

private:
  ~nsDOMDeviceStorageStat();
  PRUint64 mFreeBytes, mTotalBytes;
  nsString mState;
};

//helpers
jsval StringToJsval(nsPIDOMWindow* aWindow, nsAString& aString);
jsval nsIFileToJsval(nsPIDOMWindow* aWindow, DeviceStorageFile* aFile);
jsval InterfaceToJsval(nsPIDOMWindow* aWindow, nsISupports* aObject, const nsIID* aIID);

#ifdef MOZ_WIDGET_GONK
nsresult GetSDCardStatus(nsAString& aState);
#endif

#endif
