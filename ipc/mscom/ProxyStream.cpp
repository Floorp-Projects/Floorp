/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Move.h"
#include "mozilla/mscom/EnsureMTA.h"
#include "mozilla/mscom/ProxyStream.h"
#include "mozilla/mscom/Utils.h"
#include "mozilla/WindowsVersion.h"

#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#endif

#include <windows.h>
#include <objbase.h>
#include <shlwapi.h>

namespace mozilla {
namespace mscom {

ProxyStream::ProxyStream()
  : mGlobalLockedBuf(nullptr)
  , mHGlobal(nullptr)
  , mBufSize(0)
{
}

// GetBuffer() fails with this variant, but that's okay because we're just
// reconstructing the stream from a buffer anyway.
ProxyStream::ProxyStream(const BYTE* aInitBuf, const int aInitBufSize)
  : mStream(InitStream(aInitBuf, static_cast<const UINT>(aInitBufSize)))
  , mGlobalLockedBuf(nullptr)
  , mHGlobal(nullptr)
  , mBufSize(aInitBufSize)
{
  if (!aInitBufSize) {
    // We marshaled a nullptr. Nothing else to do here.
    return;
  }
  // NB: We can't check for a null mStream until after we have checked for
  // the zero aInitBufSize above. This is because InitStream will also fail
  // in that case, even though marshaling a nullptr is allowable.
  MOZ_ASSERT(mStream);
  if (!mStream) {
    return;
  }

  HRESULT unmarshalResult = S_OK;

  // We need to convert to an interface here otherwise we mess up const
  // correctness with IPDL. We'll request an IUnknown and then QI the
  // actual interface later.

  auto marshalFn = [&]() -> void
  {
    // OK to forget mStream when calling into this function because the stream
    // gets released even if the unmarshaling part fails.
    unmarshalResult =
      ::CoGetInterfaceAndReleaseStream(mStream.forget().take(), IID_IUnknown,
                                       getter_AddRefs(mUnmarshaledProxy));
    MOZ_ASSERT(SUCCEEDED(unmarshalResult));
  };

  if (XRE_IsParentProcess()) {
    // We'll marshal this stuff directly using the current thread, therefore its
    // proxy will reside in the same apartment as the current thread.
    marshalFn();
  } else {
    // When marshaling in child processes, we want to force the MTA.
    EnsureMTA mta(marshalFn);
  }

#if defined(MOZ_CRASHREPORTER)
  if (FAILED(unmarshalResult)) {
    nsPrintfCString hrAsStr("0x%08X", unmarshalResult);
    CrashReporter::AnnotateCrashReport(
        NS_LITERAL_CSTRING("CoGetInterfaceAndReleaseStreamFailure"), hrAsStr);
  }
#endif // defined(MOZ_CRASHREPORTER)
}

/* static */
already_AddRefed<IStream>
ProxyStream::InitStream(const BYTE* aInitBuf, const UINT aInitBufSize)
{
  if (!aInitBuf || !aInitBufSize) {
    return nullptr;
  }

  HRESULT hr;
  RefPtr<IStream> stream;

  if (IsWin8OrLater()) {
    // This function is not safe for us to use until Windows 8
    stream = already_AddRefed<IStream>(::SHCreateMemStream(aInitBuf, aInitBufSize));
    if (!stream) {
      return nullptr;
    }
  } else {
    HGLOBAL hglobal = ::GlobalAlloc(GMEM_MOVEABLE, aInitBufSize);
    if (!hglobal) {
      return nullptr;
    }

    // stream takes ownership of hglobal if this call is successful
    hr = ::CreateStreamOnHGlobal(hglobal, TRUE, getter_AddRefs(stream));
    if (FAILED(hr)) {
      ::GlobalFree(hglobal);
      return nullptr;
    }

    // The default stream size is derived from ::GlobalSize(hglobal), which due
    // to rounding may be larger than aInitBufSize. We forcibly set the correct
    // stream size here.
    ULARGE_INTEGER streamSize;
    streamSize.QuadPart = aInitBufSize;
    hr = stream->SetSize(streamSize);
    if (FAILED(hr)) {
      return nullptr;
    }

    void* streamBuf = ::GlobalLock(hglobal);
    if (!streamBuf) {
      return nullptr;
    }

    memcpy(streamBuf, aInitBuf, aInitBufSize);

    ::GlobalUnlock(hglobal);
  }

  // Ensure that the stream is rewound
  LARGE_INTEGER streamOffset;
  streamOffset.QuadPart = 0;
  hr = stream->Seek(streamOffset, STREAM_SEEK_SET, nullptr);
  if (FAILED(hr)) {
    return nullptr;
  }

  return stream.forget();
}

ProxyStream::ProxyStream(ProxyStream&& aOther)
{
  *this = mozilla::Move(aOther);
}

ProxyStream&
ProxyStream::operator=(ProxyStream&& aOther)
{
  mStream = Move(aOther.mStream);
  mGlobalLockedBuf = aOther.mGlobalLockedBuf;
  aOther.mGlobalLockedBuf = nullptr;
  mHGlobal = aOther.mHGlobal;
  aOther.mHGlobal = nullptr;
  mBufSize = aOther.mBufSize;
  aOther.mBufSize = 0;
  mUnmarshaledProxy = Move(aOther.mUnmarshaledProxy);
  return *this;
}

ProxyStream::~ProxyStream()
{
  if (mHGlobal && mGlobalLockedBuf) {
    DebugOnly<BOOL> result = ::GlobalUnlock(mHGlobal);
    MOZ_ASSERT(!result && ::GetLastError() == NO_ERROR);
    // ::GlobalFree() is called implicitly when mStream is released
  }
}

const BYTE*
ProxyStream::GetBuffer(int& aReturnedBufSize) const
{
  aReturnedBufSize = 0;
  if (!mStream) {
    return nullptr;
  }
  if (!mGlobalLockedBuf) {
    return nullptr;
  }
  aReturnedBufSize = mBufSize;
  return mGlobalLockedBuf;
}

bool
ProxyStream::GetInterface(REFIID aIID, void** aOutInterface) const
{
  // We should not have a locked buffer on this side
  MOZ_ASSERT(!mGlobalLockedBuf);
  MOZ_ASSERT(aOutInterface);

  if (!aOutInterface) {
    return false;
  }

  if (!mUnmarshaledProxy) {
    *aOutInterface = nullptr;
    return true;
  }

  HRESULT hr = E_UNEXPECTED;

  auto qiFn = [&]() -> void
  {
    hr = mUnmarshaledProxy->QueryInterface(aIID, aOutInterface);
  };

  if (XRE_IsParentProcess()) {
    qiFn();
  } else {
    // mUnmarshaledProxy requires that we execute this in the MTA
    EnsureMTA mta(qiFn);
  }

  return SUCCEEDED(hr);
}

ProxyStream::ProxyStream(REFIID aIID, IUnknown* aObject)
  : mGlobalLockedBuf(nullptr)
  , mHGlobal(nullptr)
  , mBufSize(0)
{
  if (!aObject) {
    return;
  }

  RefPtr<IStream> stream;
  HGLOBAL hglobal = NULL;
  int streamSize = 0;

  HRESULT marshalResult = S_OK;

  auto marshalFn = [&]() -> void
  {
    HRESULT hr = ::CreateStreamOnHGlobal(nullptr, TRUE, getter_AddRefs(stream));
    if (FAILED(hr)) {
      return;
    }

    hr = ::CoMarshalInterface(stream, aIID, aObject, MSHCTX_LOCAL, nullptr,
                              MSHLFLAGS_NORMAL);
    if (FAILED(hr)) {
      marshalResult = hr;
      return;
    }

    STATSTG statstg;
    hr = stream->Stat(&statstg, STATFLAG_NONAME);
    if (SUCCEEDED(hr)) {
      streamSize = static_cast<int>(statstg.cbSize.LowPart);
    }

    hr = ::GetHGlobalFromStream(stream, &hglobal);
    MOZ_ASSERT(SUCCEEDED(hr));
  };

  if (XRE_IsParentProcess()) {
    // We'll marshal this stuff directly using the current thread, therefore its
    // stub will reside in the same apartment as the current thread.
    marshalFn();
  } else {
    // When marshaling in child processes, we want to force the MTA.
    EnsureMTA mta(marshalFn);
  }

#if defined(MOZ_CRASHREPORTER)
  if (FAILED(marshalResult)) {
    nsPrintfCString hrAsStr("0x%08X", marshalResult);
    CrashReporter::AnnotateCrashReport(
        NS_LITERAL_CSTRING("CoMarshalInterfaceFailure"), hrAsStr);
  }
#endif // defined(MOZ_CRASHREPORTER)

  mStream = mozilla::Move(stream);
  mBufSize = streamSize;

  if (hglobal) {
    mGlobalLockedBuf = reinterpret_cast<BYTE*>(::GlobalLock(hglobal));
    mHGlobal = hglobal;

    // If we couldn't get the stream size directly from mStream, we may use
    // the size of the memory block allocated by the HGLOBAL, though it might
    // be larger than the actual stream size.
    if (!streamSize) {
      mBufSize = static_cast<int>(::GlobalSize(hglobal));
    }
  }
}

} // namespace mscom
} // namespace mozilla

