/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IPC_FencerUtils_h
#define IPC_FencerUtils_h

#include "ipc/IPCMessageUtils.h"
#include "nsRefPtr.h"             // for nsRefPtr

namespace mozilla {
namespace layers {

class FenceHandle {
public:
  class FdObj {
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FdObj)
    friend class FenceHandle;
  public:
    FdObj()
      : mFd(-1) {}
    explicit FdObj(int aFd)
      : mFd(aFd) {}
    int GetAndResetFd()
    {
      int fd = mFd;
      mFd = -1;
      return fd;
    }

  private:
    virtual ~FdObj() {
      if (mFd != -1) {
        close(mFd);
      }
    }

    int mFd;
  };

  FenceHandle();

  explicit FenceHandle(FdObj* aFdObj);

  bool operator==(const FenceHandle& aOther) const {
    return mFence.get() == aOther.mFence.get();
  }

  bool IsValid() const
  {
    return (mFence->mFd != -1);
  }

  void Merge(const FenceHandle& aFenceHandle);

  void TransferToAnotherFenceHandle(FenceHandle& aFenceHandle);

  already_AddRefed<FdObj> GetAndResetFdObj();

  already_AddRefed<FdObj> GetDupFdObj();

private:
  nsRefPtr<FdObj> mFence;
};

} // namespace layers
} // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::layers::FenceHandle> {
  typedef mozilla::layers::FenceHandle paramType;

  static void Write(Message* aMsg, const paramType& aParam);
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult);
};

} // namespace IPC

#endif // IPC_FencerUtils_h
