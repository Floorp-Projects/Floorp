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
#include "mozilla/TypedEnumBits.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace mscom {

enum class ProxyStreamFlags : uint32_t
{
  eDefault = 0,
  // When ePreservable is set on a ProxyStream, its caller *must* call
  // GetPreservableStream() before the ProxyStream is destroyed.
  ePreservable = 1
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(ProxyStreamFlags);

class ProxyStream final
{
public:
  ProxyStream();
  ProxyStream(REFIID aIID, IUnknown* aObject,
              ProxyStreamFlags aFlags = ProxyStreamFlags::eDefault);
  ProxyStream(REFIID aIID, const BYTE* aInitBuf, const int aInitBufSize);

  ~ProxyStream();

  // Not copyable because this would mess up the COM marshaling.
  ProxyStream(const ProxyStream& aOther) = delete;
  ProxyStream& operator=(const ProxyStream& aOther) = delete;

  ProxyStream(ProxyStream&& aOther);
  ProxyStream& operator=(ProxyStream&& aOther);

  inline bool IsValid() const
  {
    return !(mUnmarshaledProxy && mStream);
  }

  bool GetInterface(void** aOutInterface);
  const BYTE* GetBuffer(int& aReturnedBufSize) const;

  PreservedStreamPtr GetPreservedStream();

  bool operator==(const ProxyStream& aOther) const
  {
    return this == &aOther;
  }

private:
  RefPtr<IStream> mStream;
  BYTE*           mGlobalLockedBuf;
  HGLOBAL         mHGlobal;
  int             mBufSize;
  ProxyUniquePtr<IUnknown> mUnmarshaledProxy;
  bool            mPreserveStream;
};

} // namespace mscom
} // namespace mozilla

#endif // mozilla_mscom_ProxyStream_h
