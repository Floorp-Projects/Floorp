/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
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
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"
#include "DOMRequest.h"

#define DEVICESTORAGE_PICTURES   "pictures"
#define DEVICESTORAGE_VIDEOS     "videos"
#define DEVICESTORAGE_MUSIC      "music"
#define DEVICESTORAGE_APPS       "apps"
#define DEVICESTORAGE_SDCARD     "sdcard"

class DeviceStorageFile MOZ_FINAL
  : public nsISupports {
public:
  nsCOMPtr<nsIFile> mFile;
  nsString mStorageType;
  nsString mStorageName;
  nsString mRootDir;
  nsString mPath;
  bool mEditable;

  // Used when the path will be set later via SetPath.
  DeviceStorageFile(const nsAString& aStorageType,
                    const nsAString& aStorageName);
  // Used for non-enumeration purposes.
  DeviceStorageFile(const nsAString& aStorageType,
                    const nsAString& aStorageName,
                    const nsAString& aPath);
  // Used for enumerations. When you call Enumerate, you can pass in a directory to enumerate
  // and the results that are returned are relative to that directory, files related to an
  // enumeration need to know the "root of the enumeration" directory.
  DeviceStorageFile(const nsAString& aStorageType,
                    const nsAString& aStorageName,
                    const nsAString& aRootDir,
                    const nsAString& aPath);

  void SetPath(const nsAString& aPath);
  void SetEditable(bool aEditable);

  static already_AddRefed<DeviceStorageFile> CreateUnique(nsAString& aFileName,
                                                          uint32_t aFileType,
                                                          uint32_t aFileAttributes);

  NS_DECL_ISUPPORTS

  bool IsAvailable();
  bool IsComposite();
  void GetCompositePath(nsAString& aCompositePath);

  // we want to make sure that the names of file can't reach
  // outside of the type of storage the user asked for.
  bool IsSafePath();
  bool IsSafePath(const nsAString& aPath);

  void Dump(const char* label);

  nsresult Remove();
  nsresult Write(nsIInputStream* aInputStream);
  nsresult Write(InfallibleTArray<uint8_t>& bits);
  void CollectFiles(nsTArray<nsRefPtr<DeviceStorageFile> >& aFiles,
                    PRTime aSince = 0);
  void collectFilesInternal(nsTArray<nsRefPtr<DeviceStorageFile> >& aFiles,
                            PRTime aSince, nsAString& aRootPath);

  void AccumDiskUsage(uint64_t* aPicturesSoFar, uint64_t* aVideosSoFar,
                      uint64_t* aMusicSoFar, uint64_t* aTotalSoFar);

  void GetDiskFreeSpace(int64_t* aSoFar);
  void GetStatus(nsAString& aStatus);
  static void GetRootDirectoryForType(const nsAString& aStorageType,
                                      const nsAString& aStorageName,
                                      nsIFile** aFile);
private:
  void Init();
  void NormalizeFilePath();
  void AppendRelativePath(const nsAString& aPath);
  void GetStatusInternal(nsAString& aStorageName, nsAString& aStatus);
  void AccumDirectoryUsage(nsIFile* aFile,
                           uint64_t* aPicturesSoFar,
                           uint64_t* aVideosSoFar,
                           uint64_t* aMusicSoFar,
                           uint64_t* aTotalSoFar);
};

/*
  The FileUpdateDispatcher converts file-watcher-notify
  observer events to file-watcher-update events.  This is
  used to be able to broadcast events from one child to
  another child in B2G.  (f.e., if one child decides to add
  a file, we want to be able to able to send a onchange
  notifications to every other child watching that device
  storage object).

  We create this object (via GetSingleton) in two places:
    * ContentParent::Init (for IPC)
    * nsDOMDeviceStorage::Init (for non-ipc)
*/
class FileUpdateDispatcher MOZ_FINAL
  : public nsIObserver
{
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static FileUpdateDispatcher* GetSingleton();
 private:
  static mozilla::StaticRefPtr<FileUpdateDispatcher> sSingleton;
};

class nsDOMDeviceStorage MOZ_FINAL
  : public nsDOMEventTargetHelper
  , public nsIDOMDeviceStorage
  , public nsIObserver
{
public:
  typedef nsTArray<nsString> VolumeNameArray;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMDEVICESTORAGE

  NS_DECL_NSIOBSERVER
  NS_DECL_NSIDOMEVENTTARGET
  virtual void AddEventListener(const nsAString& aType,
                                nsIDOMEventListener* aListener,
                                bool aUseCapture,
                                const mozilla::dom::Nullable<bool>& aWantsUntrusted,
                                mozilla::ErrorResult& aRv) MOZ_OVERRIDE;
  virtual void RemoveEventListener(const nsAString& aType,
                                   nsIDOMEventListener* aListener,
                                   bool aUseCapture,
                                   mozilla::ErrorResult& aRv) MOZ_OVERRIDE;

  nsDOMDeviceStorage();

  nsresult Init(nsPIDOMWindow* aWindow, const nsAString& aType,
                nsTArray<nsRefPtr<nsDOMDeviceStorage> >& aStores);
  nsresult Init(nsPIDOMWindow* aWindow, const nsAString& aType,
                const nsAString& aVolName);

  bool IsAvailable();

  void SetRootDirectoryForType(const nsAString& aType, const nsAString& aVolName);

  static void CreateDeviceStorageFor(nsPIDOMWindow* aWin,
                                     const nsAString& aType,
                                     nsDOMDeviceStorage** aStore);

  static void CreateDeviceStoragesFor(nsPIDOMWindow* aWin,
                                      const nsAString& aType,
                                      nsTArray<nsRefPtr<nsDOMDeviceStorage> >& aStores,
                                      bool aCompositeComponent);
  void Shutdown();

  static void GetOrderedVolumeNames(nsTArray<nsString>& aVolumeNames);

  static void GetWritableStorageName(const nsAString& aStorageType,
                                     nsAString &aStorageName);

  static bool ParseCompositePath(const nsAString& aCompositePath,
                                 nsAString& aOutStorageName,
                                 nsAString& aOutStoragePath);
private:
  ~nsDOMDeviceStorage();

  nsresult GetInternal(const JS::Value & aName,
                       JSContext* aCx,
                       nsIDOMDOMRequest** aRetval,
                       bool aEditable);

  nsresult GetInternal(nsPIDOMWindow* aWin,
                       const nsAString& aPath,
                       mozilla::dom::DOMRequest* aRequest,
                       bool aEditable);

  nsresult DeleteInternal(nsPIDOMWindow* aWin,
                          const nsAString& aPath,
                          mozilla::dom::DOMRequest* aRequest);

  nsresult EnumerateInternal(const JS::Value& aName,
                             const JS::Value& aOptions,
                             JSContext* aCx,
                             uint8_t aArgc,
                             bool aEditable,
                             nsIDOMDOMCursor** aRetval);

  nsString mStorageType;
  nsCOMPtr<nsIFile> mRootDirectory;
  nsString mStorageName;
  bool mCompositeComponent;

  // A composite device storage object is one which front-ends for multiple
  // real storage objects. The real storage objects will each be stored in
  // mStores and will each have a unique mStorageName. The composite storage
  // object will have mStorageName == "", and mRootDirectory will be null.
  // 
  // Note that on desktop (or other non-gonk), composite storage areas
  // don't exist, and mStorageName will also be "".
  //
  // A device storage object which is stored in mStores is considered to be
  // a composite component.

  bool IsComposite() { return mStores.Length() > 0; }
  bool IsCompositeComponent() { return mCompositeComponent; }
  nsTArray<nsRefPtr<nsDOMDeviceStorage> > mStores;
  already_AddRefed<nsDOMDeviceStorage> GetStorage(const nsAString& aCompositePath,
                                                  nsAString& aOutStoragePath);
  already_AddRefed<nsDOMDeviceStorage> GetStorageByName(const nsAString &aStorageName);

  nsCOMPtr<nsIPrincipal> mPrincipal;

  bool mIsWatchingFile;
  bool mAllowedToWatchFile;

  nsresult Notify(const char* aReason, class DeviceStorageFile* aFile);

  friend class WatchFileEvent;
  friend class DeviceStorageRequest;

  class VolumeNameCache : public mozilla::RefCounted<VolumeNameCache>
  {
  public:
    nsTArray<nsString>  mVolumeNames;
  };
  static mozilla::StaticRefPtr<VolumeNameCache> sVolumeNameCache;

#ifdef MOZ_WIDGET_GONK
  void DispatchMountChangeEvent(nsAString& aVolumeName,
                                nsAString& aVolumeStatus);
#endif

  // nsIDOMDeviceStorage.type
  enum {
      DEVICE_STORAGE_TYPE_DEFAULT = 0,
      DEVICE_STORAGE_TYPE_SHARED,
      DEVICE_STORAGE_TYPE_EXTERNAL
  };
};

#endif
