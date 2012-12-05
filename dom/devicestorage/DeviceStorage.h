/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DeviceStorage_h
#define DeviceStorage_h

#include "nsIDOMDeviceStorage.h"
#include "nsIFile.h"
#include "nsIPrincipal.h"
#include "nsIObserver.h"
#include "nsDOMEventTargetHelper.h"

class DeviceStorageFile MOZ_FINAL
  : public nsISupports {
public:
  nsCOMPtr<nsIFile> mFile;
  nsString mPath;
  nsString mStorageType;
  bool mEditable;

  DeviceStorageFile(const nsAString& aStorageType, nsIFile* aFile, const nsAString& aPath);
  DeviceStorageFile(const nsAString& aStorageType, nsIFile* aFile);
  void SetPath(const nsAString& aPath);
  void SetEditable(bool aEditable);

  NS_DECL_ISUPPORTS

  // we want to make sure that the names of file can't reach
  // outside of the type of storage the user asked for.
  bool IsSafePath();

  nsresult Remove();
  nsresult Write(nsIInputStream* aInputStream);
  nsresult Write(InfallibleTArray<uint8_t>& bits);
  void CollectFiles(nsTArray<nsRefPtr<DeviceStorageFile> > &aFiles, PRTime aSince = 0);
  void collectFilesInternal(nsTArray<nsRefPtr<DeviceStorageFile> > &aFiles, PRTime aSince, nsAString& aRootPath);

  static void DirectoryDiskUsage(nsIFile* aFile, uint64_t* aSoFar, const nsAString& aStorageType);

private:
  void NormalizeFilePath();
  void AppendRelativePath();
};

class nsDOMDeviceStorage MOZ_FINAL
  : public nsIDOMDeviceStorage
  , public nsDOMEventTargetHelper
  , public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMDEVICESTORAGE

  NS_DECL_NSIOBSERVER
  NS_DECL_NSIDOMEVENTTARGET
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDOMDeviceStorage, nsDOMEventTargetHelper)

  nsDOMDeviceStorage();

  nsresult Init(nsPIDOMWindow* aWindow, const nsAString &aType);

  void SetRootDirectoryForType(const nsAString& aType);

  static void CreateDeviceStoragesFor(nsPIDOMWindow* aWin,
                                      const nsAString &aType,
                                      nsDOMDeviceStorage** aStore);
  void Shutdown();

private:
  ~nsDOMDeviceStorage();

  nsresult GetInternal(const JS::Value & aName,
                       JSContext* aCx,
                       nsIDOMDOMRequest** aRetval,
                       bool aEditable);

  nsresult EnumerateInternal(const JS::Value & aName,
                             const JS::Value & aOptions,
                             JSContext* aCx,
                             uint8_t aArgc,
                             bool aEditable,
                             nsIDOMDeviceStorageCursor** aRetval);

  nsString mStorageType;
  nsCOMPtr<nsIFile> mRootDirectory;

  nsCOMPtr<nsIPrincipal> mPrincipal;

  bool mIsWatchingFile;
  bool mAllowedToWatchFile;

  nsresult Notify(const char* aReason, class DeviceStorageFile* aFile);

  friend class WatchFileEvent;
  friend class DeviceStorageRequest;

#ifdef MOZ_WIDGET_GONK
  void DispatchMountChangeEvent(nsAString& aType);
#endif

  // nsIDOMDeviceStorage.type
  enum {
      DEVICE_STORAGE_TYPE_DEFAULT = 0,
      DEVICE_STORAGE_TYPE_SHARED,
      DEVICE_STORAGE_TYPE_EXTERNAL
  };
};

#endif
