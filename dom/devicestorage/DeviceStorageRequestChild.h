/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_devicestorage_DeviceStorageRequestChild_h
#define mozilla_dom_devicestorage_DeviceStorageRequestChild_h

#include "mozilla/dom/devicestorage/PDeviceStorageRequestChild.h"
#include "DOMRequest.h"

class DeviceStorageFile;

namespace mozilla {
namespace dom {
namespace devicestorage {

class DeviceStorageRequestChildCallback
{
  public:
    virtual void RequestComplete() = 0;
};

class DeviceStorageRequestChild : public PDeviceStorageRequestChild
{
public:
  DeviceStorageRequestChild();
  DeviceStorageRequestChild(DOMRequest* aRequest, DeviceStorageFile* aFile);
  ~DeviceStorageRequestChild();

  void SetCallback(class DeviceStorageRequestChildCallback *aCallback);

  virtual bool Recv__delete__(const DeviceStorageResponseValue& value);

private:
  nsRefPtr<DOMRequest> mRequest;
  nsRefPtr<DeviceStorageFile> mFile;

  DeviceStorageRequestChildCallback* mCallback;
};

} // namespace devicestorage
} // namespace dom
} // namespace mozilla

#endif
