/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DeviceStorage_h
#define DeviceStorage_h

#include "nsIFile.h"
#include "nsIPrincipal.h"
#include "nsIObserver.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/dom/DOMRequest.h"

#define DEVICESTORAGE_PICTURES   "pictures"
#define DEVICESTORAGE_VIDEOS     "videos"
#define DEVICESTORAGE_MUSIC      "music"
#define DEVICESTORAGE_APPS       "apps"
#define DEVICESTORAGE_SDCARD     "sdcard"
#define DEVICESTORAGE_CRASHES    "crashes"

class nsIInputStream;
class nsIOutputStream;
struct DeviceStorageFileDescriptor;

namespace mozilla {
class EventListenerManager;
namespace dom {
class Blob;
struct DeviceStorageEnumerationParameters;
class DOMCursor;
class DOMRequest;
class Promise;
class DeviceStorageFileSystem;
} // namespace dom
namespace ipc {
class FileDescriptor;
}
} // namespace mozilla

class DeviceStorageFile final
  : public nsISupports {
public:
  nsCOMPtr<nsIFile> mFile;
  nsString mStorageType;
  nsString mStorageName;
  nsString mRootDir;
  nsString mPath;
  bool mEditable;
  nsString mMimeType;
  uint64_t mLength;
  uint64_t mLastModifiedDate;

  // Used when the path will be set later via SetPath.
  DeviceStorageFile(const nsAString& aStorageType,
                    const nsAString& aStorageName);
  // Used for non-enumeration purposes.
  DeviceStorageFile(const nsAString& aStorageType,
                    const nsAString& aStorageName,
                    const nsAString& aPath);
  // Used for enumerations. When you call Enumerate, you can pass in a
  // directory to enumerate and the results that are returned are relative to
  // that directory, files related to an enumeration need to know the "root of
  // the enumeration" directory.
  DeviceStorageFile(const nsAString& aStorageType,
                    const nsAString& aStorageName,
                    const nsAString& aRootDir,
                    const nsAString& aPath);

  void SetPath(const nsAString& aPath);
  void SetEditable(bool aEditable);

  static already_AddRefed<DeviceStorageFile>
  CreateUnique(nsAString& aFileName,
               uint32_t aFileType,
               uint32_t aFileAttributes);

  NS_DECL_THREADSAFE_ISUPPORTS

  bool IsAvailable();
  void GetFullPath(nsAString& aFullPath);

  // we want to make sure that the names of file can't reach
  // outside of the type of storage the user asked for.
  bool IsSafePath();
  bool IsSafePath(const nsAString& aPath);

  void Dump(const char* label);

  nsresult Remove();
  nsresult Write(nsIInputStream* aInputStream);
  nsresult Write(InfallibleTArray<uint8_t>& bits);
  nsresult Append(nsIInputStream* aInputStream);
  nsresult Append(nsIInputStream* aInputStream,
                  nsIOutputStream* aOutputStream);
  void CollectFiles(nsTArray<nsRefPtr<DeviceStorageFile> >& aFiles,
                    PRTime aSince = 0);
  void collectFilesInternal(nsTArray<nsRefPtr<DeviceStorageFile> >& aFiles,
                            PRTime aSince, nsAString& aRootPath);

  void AccumDiskUsage(uint64_t* aPicturesSoFar, uint64_t* aVideosSoFar,
                      uint64_t* aMusicSoFar, uint64_t* aTotalSoFar);

  void GetDiskFreeSpace(int64_t* aSoFar);
  void GetStatus(nsAString& aStatus);
  void GetStorageStatus(nsAString& aStatus);
  void DoFormat(nsAString& aStatus);
  void DoMount(nsAString& aStatus);
  void DoUnmount(nsAString& aStatus);
  static void GetRootDirectoryForType(const nsAString& aStorageType,
                                      const nsAString& aStorageName,
                                      nsIFile** aFile);

  nsresult CalculateSizeAndModifiedDate();
  nsresult CalculateMimeType();
  nsresult CreateFileDescriptor(mozilla::ipc::FileDescriptor& aFileDescriptor);

private:
  ~DeviceStorageFile() {}
  void Init();
  void NormalizeFilePath();
  void AppendRelativePath(const nsAString& aPath);
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
class FileUpdateDispatcher final
  : public nsIObserver
{
  ~FileUpdateDispatcher() {}

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static FileUpdateDispatcher* GetSingleton();
 private:
  static mozilla::StaticRefPtr<FileUpdateDispatcher> sSingleton;
};

class nsDOMDeviceStorage final
  : public mozilla::DOMEventTargetHelper
  , public nsIObserver
{
  typedef mozilla::ErrorResult ErrorResult;
  typedef mozilla::dom::DeviceStorageEnumerationParameters
    EnumerationParameters;
  typedef mozilla::dom::DOMCursor DOMCursor;
  typedef mozilla::dom::DOMRequest DOMRequest;
  typedef mozilla::dom::Promise Promise;
  typedef mozilla::dom::DeviceStorageFileSystem DeviceStorageFileSystem;
public:
  typedef nsTArray<nsString> VolumeNameArray;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIOBSERVER
  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(DOMEventTargetHelper)

  void EventListenerWasAdded(const nsAString& aType,
                             ErrorResult& aRv,
                             JSCompartment* aCompartment) override;

  explicit nsDOMDeviceStorage(nsPIDOMWindow* aWindow);

  static int InstanceCount() { return sInstanceCount; }

  nsresult Init(nsPIDOMWindow* aWindow, const nsAString& aType,
                const nsAString& aVolName);

  bool IsAvailable();
  bool IsFullPath(const nsAString& aPath)
  {
    return aPath.Length() > 0 && aPath.CharAt(0) == '/';
  }

  void SetRootDirectoryForType(const nsAString& aType,
                               const nsAString& aVolName);

  // WebIDL
  nsPIDOMWindow*
  GetParentObject() const
  {
    return GetOwner();
  }
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  IMPL_EVENT_HANDLER(change)

  already_AddRefed<DOMRequest>
  Add(mozilla::dom::Blob* aBlob, ErrorResult& aRv);
  already_AddRefed<DOMRequest>
  AddNamed(mozilla::dom::Blob* aBlob, const nsAString& aPath, ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  AppendNamed(mozilla::dom::Blob* aBlob, const nsAString& aPath,
              ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  AddOrAppendNamed(mozilla::dom::Blob* aBlob, const nsAString& aPath,
                   const int32_t aRequestType, ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  Get(const nsAString& aPath, ErrorResult& aRv)
  {
    return GetInternal(aPath, false, aRv);
  }
  already_AddRefed<DOMRequest>
  GetEditable(const nsAString& aPath, ErrorResult& aRv)
  {
    return GetInternal(aPath, true, aRv);
  }
  already_AddRefed<DOMRequest>
  Delete(const nsAString& aPath, ErrorResult& aRv);

  already_AddRefed<DOMCursor>
  Enumerate(const EnumerationParameters& aOptions, ErrorResult& aRv)
  {
    return Enumerate(NullString(), aOptions, aRv);
  }
  already_AddRefed<DOMCursor>
  Enumerate(const nsAString& aPath, const EnumerationParameters& aOptions,
            ErrorResult& aRv);
  already_AddRefed<DOMCursor>
  EnumerateEditable(const EnumerationParameters& aOptions, ErrorResult& aRv)
  {
    return EnumerateEditable(NullString(), aOptions, aRv);
  }
  already_AddRefed<DOMCursor>
  EnumerateEditable(const nsAString& aPath,
                    const EnumerationParameters& aOptions, ErrorResult& aRv);

  already_AddRefed<DOMRequest> FreeSpace(ErrorResult& aRv);
  already_AddRefed<DOMRequest> UsedSpace(ErrorResult& aRv);
  already_AddRefed<DOMRequest> Available(ErrorResult& aRv);
  already_AddRefed<DOMRequest> Format(ErrorResult& aRv);
  already_AddRefed<DOMRequest> StorageStatus(ErrorResult& aRv);
  already_AddRefed<DOMRequest> Mount(ErrorResult& aRv);
  already_AddRefed<DOMRequest> Unmount(ErrorResult& aRv);

  already_AddRefed<DOMRequest> CreateFileDescriptor(const nsAString& aPath,
                                                    DeviceStorageFileDescriptor* aDSFD,
                                                    ErrorResult& aRv);

  bool CanBeMounted();
  bool CanBeFormatted();
  bool CanBeShared();
  bool IsRemovable();
  bool Default();
  void GetStorageName(nsAString& aStorageName);

  already_AddRefed<Promise>
  GetRoot(ErrorResult& aRv);

  static void
  CreateDeviceStorageFor(nsPIDOMWindow* aWin,
                         const nsAString& aType,
                         nsDOMDeviceStorage** aStore);

  static void
  CreateDeviceStoragesFor(nsPIDOMWindow* aWin,
                          const nsAString& aType,
                          nsTArray<nsRefPtr<nsDOMDeviceStorage> >& aStores);

  static void
  CreateDeviceStorageByNameAndType(nsPIDOMWindow* aWin,
                                   const nsAString& aName,
                                   const nsAString& aType,
                                   nsDOMDeviceStorage** aStore);

  void Shutdown();

  static void GetOrderedVolumeNames(nsTArray<nsString>& aVolumeNames);

  static void GetDefaultStorageName(const nsAString& aStorageType,
                                    nsAString &aStorageName);

  static bool ParseFullPath(const nsAString& aFullPath,
                            nsAString& aOutStorageName,
                            nsAString& aOutStoragePath);
private:
  ~nsDOMDeviceStorage();

  already_AddRefed<DOMRequest>
  GetInternal(const nsAString& aPath, bool aEditable, ErrorResult& aRv);

  void
  GetInternal(nsPIDOMWindow* aWin, const nsAString& aPath, DOMRequest* aRequest,
              bool aEditable);

  void
  DeleteInternal(nsPIDOMWindow* aWin, const nsAString& aPath,
                 DOMRequest* aRequest);

  already_AddRefed<DOMCursor>
  EnumerateInternal(const nsAString& aName,
                    const EnumerationParameters& aOptions, bool aEditable,
                    ErrorResult& aRv);

  static int sInstanceCount;

  nsString mStorageType;
  nsCOMPtr<nsIFile> mRootDirectory;
  nsString mStorageName;
  bool mIsShareable;
  bool mIsRemovable;

  already_AddRefed<nsDOMDeviceStorage> GetStorage(const nsAString& aFullPath,
                                                  nsAString& aOutStoragePath);
  already_AddRefed<nsDOMDeviceStorage>
    GetStorageByName(const nsAString &aStorageName);

  static already_AddRefed<nsDOMDeviceStorage>
    GetStorageByNameAndType(nsPIDOMWindow* aWin,
                            const nsAString& aStorageName,
                            const nsAString& aType);

  nsCOMPtr<nsIPrincipal> mPrincipal;

  bool mIsWatchingFile;
  bool mAllowedToWatchFile;
  bool mIsDefaultLocation;
  void DispatchDefaultChangeEvent();

  nsresult Notify(const char* aReason, class DeviceStorageFile* aFile);

  friend class WatchFileEvent;
  friend class DeviceStorageRequest;

  static mozilla::StaticAutoPtr<nsTArray<nsString>> sVolumeNameCache;

#ifdef MOZ_WIDGET_GONK
  nsString mLastStatus;
  nsString mLastStorageStatus;
  void DispatchStatusChangeEvent(nsAString& aStatus);
  void DispatchStorageStatusChangeEvent(nsAString& aStorageStatus);
#endif

  nsRefPtr<DeviceStorageFileSystem> mFileSystem;
};

#endif
