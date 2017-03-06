/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Move.h"
#include "mozilla/mscom/EnsureMTA.h"
#include "mozilla/mscom/ProxyStream.h"
#include "mozilla/mscom/Utils.h"

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
    IUnknown* rawUnmarshaledProxy = nullptr;
    // OK to forget mStream when calling into this function because the stream
    // gets released even if the unmarshaling part fails.
    unmarshalResult =
      ::CoGetInterfaceAndReleaseStream(mStream.forget().take(), IID_IUnknown,
                                       (void**)&rawUnmarshaledProxy);
    MOZ_ASSERT(SUCCEEDED(unmarshalResult));
    mUnmarshaledProxy.reset(rawUnmarshaledProxy);
  };

  if (XRE_IsParentProcess()) {
    // We'll marshal this stuff directly using the current thread, therefore its
    // proxy will reside in the same apartment as the current thread.
    marshalFn();
  } else {
    // When marshaling in child processes, we want to force the MTA.
    EnsureMTA mta(marshalFn);
  }

#ifdef MOZ_CRASHREPORTER
  if (FAILED(unmarshalResult)) {
    nsPrintfCString hrAsStr("0x%08X", unmarshalResult);
    CrashReporter::AnnotateCrashReport(
        NS_LITERAL_CSTRING("CoGetInterfaceAndReleaseStreamFailure"), hrAsStr);
  }
#endif
}

already_AddRefed<IStream>
ProxyStream::InitStream(const BYTE* aInitBuf, const UINT aInitBufSize)
{
  return already_AddRefed<IStream>(::SHCreateMemStream(aInitBuf, aInitBufSize));
}

ProxyStream::ProxyStream(ProxyStream&& aOther)
{
  *this = mozilla::Move(aOther);
}

ProxyStream&
ProxyStream::operator=(ProxyStream&& aOther)
{
  mStream = mozilla::Move(aOther.mStream);
  mGlobalLockedBuf = aOther.mGlobalLockedBuf;
  aOther.mGlobalLockedBuf = nullptr;
  mHGlobal = aOther.mHGlobal;
  aOther.mHGlobal = nullptr;
  mBufSize = aOther.mBufSize;
  aOther.mBufSize = 0;
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
  RefPtr<IStream> stream;
  HGLOBAL hglobal = NULL;

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

#ifdef MOZ_CRASHREPORTER
  if (FAILED(marshalResult)) {
    nsPrintfCString hrAsStr("0x%08X", marshalResult);
    CrashReporter::AnnotateCrashReport(
        NS_LITERAL_CSTRING("CoMarshalInterfaceFailure"), hrAsStr);
  }
#endif

  mStream = mozilla::Move(stream);
  if (hglobal) {
    mGlobalLockedBuf = reinterpret_cast<BYTE*>(::GlobalLock(hglobal));
    mHGlobal = hglobal;
    mBufSize = static_cast<int>(::GlobalSize(hglobal));
  }
}

} // namespace mscom
} // namespace mozilla

