/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "DeviceStorageRequestChild.h"
#include "DeviceStorageFileDescriptor.h"
#include "nsDeviceStorage.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/ipc/BlobChild.h"

namespace mozilla {
namespace dom {
namespace devicestorage {

DeviceStorageRequestChild::DeviceStorageRequestChild()
{
  MOZ_COUNT_CTOR(DeviceStorageRequestChild);
}

DeviceStorageRequestChild::DeviceStorageRequestChild(DeviceStorageRequest* aRequest)
  : mRequest(aRequest)
{
  MOZ_ASSERT(aRequest);
  MOZ_COUNT_CTOR(DeviceStorageRequestChild);
}

DeviceStorageRequestChild::~DeviceStorageRequestChild() {
  MOZ_COUNT_DTOR(DeviceStorageRequestChild);
}

mozilla::ipc::IPCResult
DeviceStorageRequestChild::
  Recv__delete__(const DeviceStorageResponseValue& aValue)
{
  switch (aValue.type()) {

    case DeviceStorageResponseValue::TErrorResponse:
    {
      DS_LOG_INFO("error %u", mRequest->GetId());
      ErrorResponse r = aValue;
      mRequest->Reject(r.error());
      break;
    }

    case DeviceStorageResponseValue::TSuccessResponse:
    {
      DS_LOG_INFO("success %u", mRequest->GetId());
      nsString fullPath;
      mRequest->GetFile()->GetFullPath(fullPath);
      mRequest->Resolve(fullPath);
      break;
    }

    case DeviceStorageResponseValue::TFileDescriptorResponse:
    {
      DS_LOG_INFO("fd %u", mRequest->GetId());
      FileDescriptorResponse r = aValue;

      DeviceStorageFile* file = mRequest->GetFile();
      DeviceStorageFileDescriptor* descriptor = mRequest->GetFileDescriptor();
      nsString fullPath;
      file->GetFullPath(fullPath);
      descriptor->mDSFile = file;
      descriptor->mFileDescriptor = r.fileDescriptor();
      mRequest->Resolve(fullPath);
      break;
    }

    case DeviceStorageResponseValue::TBlobResponse:
    {
      DS_LOG_INFO("blob %u", mRequest->GetId());
      BlobResponse r = aValue;
      BlobChild* actor = static_cast<BlobChild*>(r.blobChild());
      RefPtr<BlobImpl> blobImpl = actor->GetBlobImpl();
      mRequest->Resolve(blobImpl.get());
      break;
    }

    case DeviceStorageResponseValue::TFreeSpaceStorageResponse:
    {
      DS_LOG_INFO("free %u", mRequest->GetId());
      FreeSpaceStorageResponse r = aValue;
      mRequest->Resolve(r.freeBytes());
      break;
    }

    case DeviceStorageResponseValue::TUsedSpaceStorageResponse:
    {
      DS_LOG_INFO("used %u", mRequest->GetId());
      UsedSpaceStorageResponse r = aValue;
      mRequest->Resolve(r.usedBytes());
      break;
    }

    case DeviceStorageResponseValue::TFormatStorageResponse:
    {
      DS_LOG_INFO("format %u", mRequest->GetId());
      FormatStorageResponse r = aValue;
      mRequest->Resolve(r.mountState());
      break;
    }

    case DeviceStorageResponseValue::TMountStorageResponse:
    {
      DS_LOG_INFO("mount %u", mRequest->GetId());
      MountStorageResponse r = aValue;
      mRequest->Resolve(r.storageStatus());
      break;
    }

    case DeviceStorageResponseValue::TUnmountStorageResponse:
    {
      DS_LOG_INFO("unmount %u", mRequest->GetId());
      UnmountStorageResponse r = aValue;
      mRequest->Resolve(r.storageStatus());
      break;
    }

    case DeviceStorageResponseValue::TEnumerationResponse:
    {
      DS_LOG_INFO("enumerate %u", mRequest->GetId());
      EnumerationResponse r = aValue;
      auto request = static_cast<DeviceStorageCursorRequest*>(mRequest.get());
      uint32_t count = r.paths().Length();
      request->AddFiles(count);
      for (uint32_t i = 0; i < count; i++) {
        RefPtr<DeviceStorageFile> dsf
          = new DeviceStorageFile(r.type(), r.paths()[i].storageName(),
                                  r.rootdir(), r.paths()[i].name());
        request->AddFile(dsf.forget());
      }
      request->Continue();
      break;
    }

    default:
    {
      DS_LOG_ERROR("unknown %u", mRequest->GetId());
      NS_RUNTIMEABORT("not reached");
      break;
    }
  }
  return IPC_OK();
}

} // namespace devicestorage
} // namespace dom
} // namespace mozilla
