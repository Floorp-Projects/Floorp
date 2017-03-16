/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_ProxyStream_h
#define mozilla_mscom_ProxyStream_h

#include "ipc/IPCMessageUtils.h"

#include "mozilla/mscom/Ptr.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace mscom {

class ProxyStream final
{
public:
  ProxyStream();
  ProxyStream(REFIID aIID, IUnknown* aObject);
  ProxyStream(const BYTE* aInitBuf, const int aInitBufSize);

  ~ProxyStream();

  // Not copyable because this would mess up the COM marshaling.
  ProxyStream(const ProxyStream& aOther) = delete;
  ProxyStream& operator=(const ProxyStream& aOther) = delete;

  ProxyStream(ProxyStream&& aOther);
  ProxyStream& operator=(ProxyStream&& aOther);

  inline bool IsValid() const
  {
    return !(mStream && mUnmarshaledProxy);
  }

  bool GetInterface(REFIID aIID, void** aOutInterface) const;
  const BYTE* GetBuffer(int& aReturnedBufSize) const;

  bool operator==(const ProxyStream& aOther) const
  {
    return this == &aOther;
  }

private:
  static already_AddRefed<IStream> InitStream(const BYTE* aInitBuf,
                                              const UINT aInitBufSize);

private:
  RefPtr<IStream> mStream;
  BYTE*           mGlobalLockedBuf;
  HGLOBAL         mHGlobal;
  int             mBufSize;
  ProxyUniquePtr<IUnknown> mUnmarshaledProxy;
};

} // namespace mscom
} // namespace mozilla

#endif // mozilla_mscom_ProxyStream_h
