/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_StructStream_h
#define mozilla_mscom_StructStream_h

#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"
#include "nscore.h"

#include <memory.h>
#include <midles.h>
#include <objidl.h>
#include <rpc.h>

/**
 * This code is used for (de)serializing data structures that have been
 * declared using midl, thus allowing us to use Microsoft RPC for marshaling
 * data for our COM handlers that may run in other processes that are not ours.
 */

namespace mozilla {
namespace mscom {

namespace detail {

typedef ULONG EncodedLenT;

} // namespace detail

class MOZ_NON_TEMPORARY_CLASS StructToStream
{
public:
  /**
   * This constructor variant represents an empty/null struct to be serialized.
   */
  StructToStream()
    : mStatus(RPC_S_OK)
    , mHandle(nullptr)
    , mBuffer(nullptr)
    , mEncodedLen(0)
  {
  }

  template <typename StructT>
  StructToStream(StructT& aSrcStruct, void (*aEncodeFnPtr)(handle_t, StructT*))
    : mStatus(RPC_X_INVALID_BUFFER)
    , mHandle(nullptr)
    , mBuffer(nullptr)
    , mEncodedLen(0)
  {
    mStatus = ::MesEncodeDynBufferHandleCreate(&mBuffer, &mEncodedLen,
                                               &mHandle);
    if (mStatus != RPC_S_OK) {
      return;
    }

    MOZ_SEH_TRY {
      aEncodeFnPtr(mHandle, &aSrcStruct);
    } MOZ_SEH_EXCEPT(EXCEPTION_EXECUTE_HANDLER) {
      mStatus = ::RpcExceptionCode();
      return;
    }

    if (!mBuffer || !mEncodedLen) {
      mStatus = RPC_X_NO_MEMORY;
      return;
    }
  }

  ~StructToStream()
  {
    if (mHandle) {
      ::MesHandleFree(mHandle);
    }
  }

  static unsigned long GetEmptySize()
  {
    return sizeof(detail::EncodedLenT);
  }

  static HRESULT WriteEmpty(IStream* aDestStream)
  {
    StructToStream emptyStruct;
    return emptyStruct.Write(aDestStream);
  }

  explicit operator bool() const
  {
    return mStatus == RPC_S_OK;
  }

  bool IsEmpty() const
  {
    return mStatus == RPC_S_OK && !mEncodedLen;
  }

  unsigned long GetSize() const
  {
    return sizeof(mEncodedLen) + mEncodedLen;
  }

  HRESULT Write(IStream* aDestStream)
  {
    if (!aDestStream) {
      return E_INVALIDARG;
    }
    if (mStatus != RPC_S_OK) {
      return E_FAIL;
    }

    ULONG bytesWritten;
    HRESULT hr = aDestStream->Write(&mEncodedLen, sizeof(mEncodedLen),
                                    &bytesWritten);
    if (FAILED(hr)) {
      return hr;
    }
    if (bytesWritten != sizeof(mEncodedLen)) {
      return E_UNEXPECTED;
    }

    if (mBuffer && mEncodedLen) {
      hr = aDestStream->Write(mBuffer, mEncodedLen, &bytesWritten);
      if (FAILED(hr)) {
        return hr;
      }
      if (bytesWritten != mEncodedLen) {
        return E_UNEXPECTED;
      }
    }

    return hr;
  }

  StructToStream(const StructToStream&) = delete;
  StructToStream(StructToStream&&) = delete;
  StructToStream& operator=(const StructToStream&) = delete;
  StructToStream& operator=(StructToStream&&) = delete;

private:
  RPC_STATUS          mStatus;
  handle_t            mHandle;
  char*               mBuffer;
  detail::EncodedLenT mEncodedLen;
};

class MOZ_NON_TEMPORARY_CLASS StructFromStream
{
  struct AlignedFreeDeleter
  {
    void operator()(void* aPtr)
    {
      ::_aligned_free(aPtr);
    }
  };

  static const detail::EncodedLenT kRpcReqdBufAlignment = 8;

public:
  explicit StructFromStream(IStream* aStream)
    : mStatus(RPC_X_INVALID_BUFFER)
    , mHandle(nullptr)
  {
    MOZ_ASSERT(aStream);

    // Read the length of the encoded data first
    detail::EncodedLenT encodedLen = 0;
    ULONG bytesRead = 0;
    HRESULT hr = aStream->Read(&encodedLen, sizeof(encodedLen), &bytesRead);
    if (FAILED(hr)) {
      return;
    }

    // NB: Some implementations of IStream return S_FALSE to indicate EOF,
    // other implementations return S_OK and set the number of bytes read to 0.
    // We must handle both.
    if (hr == S_FALSE || !bytesRead) {
      mStatus = RPC_S_OBJECT_NOT_FOUND;
      return;
    }

    if (bytesRead != sizeof(encodedLen)) {
      return;
    }

    if (!encodedLen) {
      mStatus = RPC_S_OBJECT_NOT_FOUND;
      return;
    }

    MOZ_ASSERT(encodedLen % kRpcReqdBufAlignment == 0);
    if (encodedLen % kRpcReqdBufAlignment) {
      return;
    }

    // This memory allocation is fallible
    mEncodedBuffer.reset(static_cast<char*>(
          ::_aligned_malloc(encodedLen, kRpcReqdBufAlignment)));
    if (!mEncodedBuffer) {
      return;
    }

    ULONG bytesReadFromStream = 0;
    hr = aStream->Read(mEncodedBuffer.get(), encodedLen, &bytesReadFromStream);
    if (FAILED(hr) || bytesReadFromStream != encodedLen) {
      return;
    }

    mStatus = ::MesDecodeBufferHandleCreate(mEncodedBuffer.get(), encodedLen,
                                            &mHandle);
  }

  ~StructFromStream()
  {
    if (mHandle) {
      ::MesHandleFree(mHandle);
    }
  }

  explicit operator bool() const
  {
    return mStatus == RPC_S_OK || IsEmpty();
  }

  bool IsEmpty() const { return mStatus == RPC_S_OBJECT_NOT_FOUND; }

  template <typename StructT>
  bool Read(StructT* aDestStruct, void (*aDecodeFnPtr)(handle_t, StructT*))
  {
    if (!aDestStruct || !aDecodeFnPtr || mStatus != RPC_S_OK) {
      return false;
    }

    // NB: Deserialization will fail with BSTRs unless the destination data
    //     is zeroed out!
    ZeroMemory(aDestStruct, sizeof(StructT));

    MOZ_SEH_TRY {
      aDecodeFnPtr(mHandle, aDestStruct);
    } MOZ_SEH_EXCEPT(EXCEPTION_EXECUTE_HANDLER) {
      mStatus = ::RpcExceptionCode();
      return false;
    }

    return true;
  }

  StructFromStream(const StructFromStream&) = delete;
  StructFromStream(StructFromStream&&) = delete;
  StructFromStream& operator=(const StructFromStream&) = delete;
  StructFromStream& operator=(StructFromStream&&) = delete;

private:
  RPC_STATUS                          mStatus;
  handle_t                            mHandle;
  UniquePtr<char, AlignedFreeDeleter> mEncodedBuffer;
};

} // namespace mscom
} // namespace mozilla

#endif // mozilla_mscom_StructStream_h
