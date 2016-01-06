/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IPC_GonkNativeHandle_h
#define IPC_GonkNativeHandle_h

#ifdef MOZ_WIDGET_GONK
#include <cutils/native_handle.h>
#endif

#include "mozilla/RefPtr.h"             // for RefPtr
#include "nsISupportsImpl.h"

namespace mozilla {
namespace layers {

#ifdef MOZ_WIDGET_GONK
// GonkNativeHandle wraps android's native_handle_t and is used to support
// android's sideband stream.
// The sideband stream is a device-specific mechanism for passing buffers
// to hwcomposer. It is used to render TV streams and DRM protected streams.
// The native_handle_t represents device-specific kernel objects on android.
class GonkNativeHandle {
public:
  class NhObj {
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(NhObj)
    friend class GonkNativeHandle;
  public:
    NhObj()
      : mHandle(nullptr) {}
    explicit NhObj(native_handle_t* aHandle)
      : mHandle(aHandle) {}
    native_handle_t* GetAndResetNativeHandle()
    {
      native_handle_t* handle = mHandle;
      mHandle = nullptr;
      return handle;
    }

  private:
    virtual ~NhObj() {
      if (mHandle) {
        native_handle_close(mHandle);
        native_handle_delete(mHandle);
      }
    }

    native_handle_t* mHandle;
  };

  GonkNativeHandle();

  explicit GonkNativeHandle(NhObj* aNhObj);

  bool operator==(const GonkNativeHandle& aOther) const {
    return mNhObj.get() == aOther.mNhObj.get();
  }

  bool IsValid() const
  {
    return mNhObj && mNhObj->mHandle;
  }

  void TransferToAnother(GonkNativeHandle& aHandle);

  already_AddRefed<NhObj> GetAndResetNhObj();

  already_AddRefed<NhObj> GetDupNhObj();

  // Return non owning handle.
  native_handle_t* GetRawNativeHandle() const
  {
    if (mNhObj) {
      return mNhObj->mHandle;
    }
    return nullptr;
  }

private:
  RefPtr<NhObj> mNhObj;
};
#else
struct GonkNativeHandle {
  bool operator==(const GonkNativeHandle&) const { return false; }
};
#endif

} // namespace layers
} // namespace mozilla

#endif // IPC_GonkNativeHandle_h
