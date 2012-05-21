/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDeviceStorage_h
#define nsDeviceStorage_h

class nsPIDOMWindow;

#include "nsIClassInfo.h"
#include "nsIDOMDeviceStorage.h"
#include "nsIDOMDeviceStorageCursor.h"
#include "nsIDOMWindow.h"
#include "nsIURI.h"

#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDOMClassInfoID.h"
#include "nsString.h"
#include "nsWeakPtr.h"
#include "nsInterfaceHashtable.h"

class nsDOMDeviceStorage : public nsIDOMDeviceStorage
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMDEVICESTORAGE

  nsDOMDeviceStorage();

  nsresult Init(nsPIDOMWindow* aWindow, const nsAString &aType, const PRInt32 aIndex);

  PRInt32 SetRootFileForType(const nsAString& aType, const PRInt32 aIndex);

  static void CreateDeviceStoragesFor(nsPIDOMWindow* aWin, const nsAString &aType, nsIVariant** _retval);

private:
  ~nsDOMDeviceStorage();


  nsresult GetInternal(const JS::Value & aName, JSContext* aCx, nsIDOMDOMRequest * *_retval NS_OUTPARAM, bool aEditable);
  nsresult EnumerateInternal(const nsAString & aName, nsIDOMDeviceStorageCursor * *_retval NS_OUTPARAM, bool aEditable);

  PRInt32 mStorageType;
  nsCOMPtr<nsIFile> mFile;

  nsWeakPtr mOwner;
  nsCOMPtr<nsIURI> mURI;

  // nsIDOMDeviceStorage.type
  enum {
      DEVICE_STORAGE_TYPE_DEFAULT = 0,
      DEVICE_STORAGE_TYPE_SHARED,
      DEVICE_STORAGE_TYPE_EXTERNAL,
  };
};

#endif
