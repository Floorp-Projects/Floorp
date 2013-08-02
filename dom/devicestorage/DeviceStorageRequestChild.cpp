/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "DeviceStorageRequestChild.h"
#include "nsDeviceStorage.h"
#include "nsDOMFile.h"
#include "mozilla/dom/ipc/Blob.h"

namespace mozilla {
namespace dom {
namespace devicestorage {

DeviceStorageRequestChild::DeviceStorageRequestChild()
  : mCallback(nullptr)
{
  MOZ_COUNT_CTOR(DeviceStorageRequestChild);
}

DeviceStorageRequestChild::DeviceStorageRequestChild(DOMRequest* aRequest,
                                                     DeviceStorageFile* aFile)
  : mRequest(aRequest)
  , mFile(aFile)
  , mCallback(nullptr)
{
  MOZ_COUNT_CTOR(DeviceStorageRequestChild);
}

DeviceStorageRequestChild::~DeviceStorageRequestChild() {
  MOZ_COUNT_DTOR(DeviceStorageRequestChild);
}

bool
DeviceStorageRequestChild::Recv__delete__(const DeviceStorageResponseValue& aValue)
{
  if (mCallback) {
    mCallback->RequestComplete();
    mCallback = nullptr;
  }

  switch (aValue.type()) {

    case DeviceStorageResponseValue::TErrorResponse:
    {
      ErrorResponse r = aValue;
      mRequest->FireError(r.error());
      break;
    }

    case DeviceStorageResponseValue::TSuccessResponse:
    {
      nsString fullPath;
      mFile->GetFullPath(fullPath);
      AutoJSContext cx;
      JS::Rooted<JS::Value> result(cx,
        StringToJsval(mRequest->GetOwner(), fullPath));
      mRequest->FireSuccess(result);
      break;
    }

    case DeviceStorageResponseValue::TBlobResponse:
    {
      BlobResponse r = aValue;
      BlobChild* actor = static_cast<BlobChild*>(r.blobChild());
      nsCOMPtr<nsIDOMBlob> blob = actor->GetBlob();

      nsCOMPtr<nsIDOMFile> file = do_QueryInterface(blob);
      AutoJSContext cx;
      JS::Rooted<JS::Value> result(cx,
        InterfaceToJsval(mRequest->GetOwner(), file, &NS_GET_IID(nsIDOMFile)));
      mRequest->FireSuccess(result);
      break;
    }

    case DeviceStorageResponseValue::TFreeSpaceStorageResponse:
    {
      FreeSpaceStorageResponse r = aValue;
      AutoJSContext cx;
      JS::Rooted<JS::Value> result(cx, JS_NumberValue(double(r.freeBytes())));
      mRequest->FireSuccess(result);
      break;
    }

    case DeviceStorageResponseValue::TUsedSpaceStorageResponse:
    {
      UsedSpaceStorageResponse r = aValue;
      AutoJSContext cx;
      JS::Rooted<JS::Value> result(cx, JS_NumberValue(double(r.usedBytes())));
      mRequest->FireSuccess(result);
      break;
    }

    case DeviceStorageResponseValue::TAvailableStorageResponse:
    {
      AvailableStorageResponse r = aValue;
      AutoJSContext cx;
      JS::Rooted<JS::Value> result(
        cx, StringToJsval(mRequest->GetOwner(), r.mountState()));
      mRequest->FireSuccess(result);
      break;
    }

    case DeviceStorageResponseValue::TEnumerationResponse:
    {
      EnumerationResponse r = aValue;
      nsDOMDeviceStorageCursor* cursor = static_cast<nsDOMDeviceStorageCursor*>(mRequest.get());

      uint32_t count = r.paths().Length();
      for (uint32_t i = 0; i < count; i++) {
        nsRefPtr<DeviceStorageFile> dsf = new DeviceStorageFile(r.type(),
                                                                r.paths()[i].storageName(),
                                                                r.rootdir(),
                                                                r.paths()[i].name());
        cursor->mFiles.AppendElement(dsf);
      }

      nsCOMPtr<ContinueCursorEvent> event = new ContinueCursorEvent(cursor);
      event->Continue();
      break;
    }

    default:
    {
      NS_RUNTIMEABORT("not reached");
      break;
    }
  }
  return true;
}

void
DeviceStorageRequestChild::SetCallback(DeviceStorageRequestChildCallback *aCallback)
{
  mCallback = aCallback;
}

} // namespace devicestorage
} // namespace dom
} // namespace mozilla
