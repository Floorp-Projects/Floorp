/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DeviceStorage_h
#define DeviceStorage_h

#include "nsIDOMDeviceStorage.h"
#include "nsIFile.h"
#include "nsIObserver.h"
#include "nsDOMEventTargetHelper.h"

class nsDOMDeviceStorage MOZ_FINAL
  : public nsIDOMDeviceStorage
  , public nsIFileUpdateListener
  , public nsDOMEventTargetHelper
  , public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMDEVICESTORAGE

  NS_DECL_NSIFILEUPDATELISTENER
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIDOMEVENTTARGET
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDOMDeviceStorage, nsDOMEventTargetHelper)
  NS_DECL_EVENT_HANDLER(change)

  nsDOMDeviceStorage();

  nsresult Init(nsPIDOMWindow* aWindow, const nsAString &aType);

  void SetRootFileForType(const nsAString& aType);

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
                             PRUint8 aArgc, 
                             bool aEditable, 
                             nsIDOMDeviceStorageCursor** aRetval);

  PRInt32 mStorageType;
  nsCOMPtr<nsIFile> mFile;

  nsCOMPtr<nsIURI> mURI;

  friend class WatchFileEvent;
  friend class DeviceStorageRequest;

  bool  mIsWatchingFile;

  // nsIDOMDeviceStorage.type
  enum {
      DEVICE_STORAGE_TYPE_DEFAULT = 0,
      DEVICE_STORAGE_TYPE_SHARED,
      DEVICE_STORAGE_TYPE_EXTERNAL
  };
};

#endif
