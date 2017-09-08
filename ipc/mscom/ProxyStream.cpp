/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Move.h"
#if defined(ACCESSIBILITY) && defined(MOZ_CRASHREPORTER)
#include "HandlerData.h"
#include "mozilla/a11y/Platform.h"
#include "mozilla/mscom/ActivationContext.h"
#endif // defined(ACCESSIBILITY) && defined(MOZ_CRASHREPORTER)
#include "mozilla/mscom/EnsureMTA.h"
#include "mozilla/mscom/ProxyStream.h"
#include "mozilla/mscom/Utils.h"

#if defined(MOZ_CRASHREPORTER)
#include "mozilla/mscom/Objref.h"
#include "nsExceptionHandler.h"
#include "nsPrintfCString.h"
#include "RegistrationAnnotator.h"
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
  , mPreserveStream(false)
{
}

// GetBuffer() fails with this variant, but that's okay because we're just
// reconstructing the stream from a buffer anyway.
ProxyStream::ProxyStream(REFIID aIID, const BYTE* aInitBuf,
                         const int aInitBufSize)
  : mGlobalLockedBuf(nullptr)
  , mHGlobal(nullptr)
  , mBufSize(aInitBufSize)
  , mPreserveStream(false)
{
#if defined(MOZ_CRASHREPORTER)
  NS_NAMED_LITERAL_CSTRING(kCrashReportKey, "ProxyStreamUnmarshalStatus");
#endif

  if (!aInitBufSize) {
#if defined(MOZ_CRASHREPORTER)
    CrashReporter::AnnotateCrashReport(kCrashReportKey,
                                       NS_LITERAL_CSTRING("!aInitBufSize"));
#endif // defined(MOZ_CRASHREPORTER)
    // We marshaled a nullptr. Nothing else to do here.
    return;
  }

  HRESULT createStreamResult = CreateStream(aInitBuf, aInitBufSize,
                                            getter_AddRefs(mStream));
  if (FAILED(createStreamResult)) {
#if defined(MOZ_CRASHREPORTER)
    nsPrintfCString hrAsStr("0x%08X", createStreamResult);
    CrashReporter::AnnotateCrashReport(kCrashReportKey, hrAsStr);
#endif // defined(MOZ_CRASHREPORTER)
    return;
  }

  // NB: We can't check for a null mStream until after we have checked for
  // the zero aInitBufSize above. This is because InitStream will also fail
  // in that case, even though marshaling a nullptr is allowable.
  MOZ_ASSERT(mStream);
  if (!mStream) {
#if defined(MOZ_CRASHREPORTER)
    CrashReporter::AnnotateCrashReport(kCrashReportKey,
                                       NS_LITERAL_CSTRING("!mStream"));
#endif // defined(MOZ_CRASHREPORTER)
    return;
  }

#if defined(ACCESSIBILITY) && defined(MOZ_CRASHREPORTER)
  const uint32_t expectedStreamLen = GetOBJREFSize(WrapNotNull(mStream));
  nsAutoCString strActCtx;
#endif // defined(ACCESSIBILITY) && defined(MOZ_CRASHREPORTER)

  HRESULT unmarshalResult = S_OK;

  // We need to convert to an interface here otherwise we mess up const
  // correctness with IPDL. We'll request an IUnknown and then QI the
  // actual interface later.

#if defined(ACCESSIBILITY) && defined(MOZ_CRASHREPORTER)
  auto marshalFn = [this, &strActCtx, &unmarshalResult, &aIID]() -> void
#else
  auto marshalFn = [this, &unmarshalResult, &aIID]() -> void
#endif // defined(ACCESSIBILITY) && defined(MOZ_CRASHREPORTER)
  {
#if defined(ACCESSIBILITY) && defined(MOZ_CRASHREPORTER)
    auto curActCtx = ActivationContext::GetCurrent();
    if (curActCtx.isOk()) {
      strActCtx.AppendPrintf("0x%p", curActCtx.unwrap());
    } else {
      strActCtx.AppendPrintf("HRESULT 0x%08X", curActCtx.unwrapErr());
    }
#endif // defined(ACCESSIBILITY) && defined(MOZ_CRASHREPORTER)

    unmarshalResult =
      ::CoUnmarshalInterface(mStream, aIID, getter_AddRefs(mUnmarshaledProxy));
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

  mStream = nullptr;

#if defined(MOZ_CRASHREPORTER)
  if (FAILED(unmarshalResult) || !mUnmarshaledProxy) {
    nsPrintfCString hrAsStr("0x%08X", unmarshalResult);
    CrashReporter::AnnotateCrashReport(
        NS_LITERAL_CSTRING("CoUnmarshalInterfaceResult"), hrAsStr);
    AnnotateInterfaceRegistration(aIID);
    if (!mUnmarshaledProxy) {
      CrashReporter::AnnotateCrashReport(kCrashReportKey,
                                         NS_LITERAL_CSTRING("!mUnmarshaledProxy"));
    }

#if defined(ACCESSIBILITY)
    AnnotateClassRegistration(CLSID_AccessibleHandler);
    CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("UnmarshalActCtx"),
                                       strActCtx);
    CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("A11yHandlerRegistered"),
                                       a11y::IsHandlerRegistered() ?
                                       NS_LITERAL_CSTRING("true") :
                                       NS_LITERAL_CSTRING("false"));

    nsAutoCString strExpectedStreamLen;
    strExpectedStreamLen.AppendInt(expectedStreamLen);
    CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("ExpectedStreamLen"),
                                       strExpectedStreamLen);

    nsAutoCString actualStreamLen;
    actualStreamLen.AppendInt(aInitBufSize);
    CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("ActualStreamLen"),
                                       actualStreamLen);
#endif // defined(ACCESSIBILITY)
  }
#endif // defined(MOZ_CRASHREPORTER)
}

ProxyStream::ProxyStream(ProxyStream&& aOther)
  : mGlobalLockedBuf(nullptr)
  , mHGlobal(nullptr)
  , mBufSize(0)
  , mPreserveStream(false)
{
  *this = mozilla::Move(aOther);
}

ProxyStream&
ProxyStream::operator=(ProxyStream&& aOther)
{
  if (mHGlobal && mGlobalLockedBuf) {
    DebugOnly<BOOL> result = ::GlobalUnlock(mHGlobal);
    MOZ_ASSERT(!result && ::GetLastError() == NO_ERROR);
  }

  mStream = Move(aOther.mStream);

  mGlobalLockedBuf = aOther.mGlobalLockedBuf;
  aOther.mGlobalLockedBuf = nullptr;

  // ::GlobalFree() was called implicitly when mStream was replaced.
  mHGlobal = aOther.mHGlobal;
  aOther.mHGlobal = nullptr;

  mBufSize = aOther.mBufSize;
  aOther.mBufSize = 0;

  mUnmarshaledProxy = Move(aOther.mUnmarshaledProxy);

  mPreserveStream = aOther.mPreserveStream;
  return *this;
}

ProxyStream::~ProxyStream()
{
  if (mHGlobal && mGlobalLockedBuf) {
    DebugOnly<BOOL> result = ::GlobalUnlock(mHGlobal);
    MOZ_ASSERT(!result && ::GetLastError() == NO_ERROR);
    // ::GlobalFree() is called implicitly when mStream is released
  }

  // If this assert triggers then we will be leaking a marshaled proxy!
  // Call GetPreservedStream to obtain a preservable stream and then save it
  // until the proxy is no longer needed.
  MOZ_ASSERT(!mPreserveStream);
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

PreservedStreamPtr
ProxyStream::GetPreservedStream()
{
  MOZ_ASSERT(mStream);
  MOZ_ASSERT(mHGlobal);

  if (!mStream || !mPreserveStream) {
    return nullptr;
  }

  // Clone the stream so that the result has a distinct seek pointer.
  RefPtr<IStream> cloned;
  HRESULT hr = mStream->Clone(getter_AddRefs(cloned));
  if (FAILED(hr)) {
    return nullptr;
  }

  // Ensure the stream is rewound. We do this because CoReleaseMarshalData needs
  // the stream to be pointing to the beginning of the marshal data.
  LARGE_INTEGER pos;
  pos.QuadPart = 0LL;
  hr = cloned->Seek(pos, STREAM_SEEK_SET, nullptr);
  if (FAILED(hr)) {
    return nullptr;
  }

  mPreserveStream = false;
  return ToPreservedStreamPtr(Move(cloned));
}

bool
ProxyStream::GetInterface(void** aOutInterface)
{
  // We should not have a locked buffer on this side
  MOZ_ASSERT(!mGlobalLockedBuf);
  MOZ_ASSERT(aOutInterface);

  if (!aOutInterface) {
    return false;
  }

  *aOutInterface = mUnmarshaledProxy.release();
  return true;
}

ProxyStream::ProxyStream(REFIID aIID, IUnknown* aObject,
                         ProxyStreamFlags aFlags)
  : mGlobalLockedBuf(nullptr)
  , mHGlobal(nullptr)
  , mBufSize(0)
  , mPreserveStream(aFlags & ProxyStreamFlags::ePreservable)
{
  if (!aObject) {
    return;
  }

  RefPtr<IStream> stream;
  HGLOBAL hglobal = NULL;
  int streamSize = 0;
  DWORD mshlFlags = mPreserveStream ? MSHLFLAGS_TABLESTRONG : MSHLFLAGS_NORMAL;

  HRESULT createStreamResult = S_OK;
  HRESULT marshalResult = S_OK;
  HRESULT statResult = S_OK;
  HRESULT getHGlobalResult = S_OK;

  auto marshalFn = [&]() -> void
  {
    createStreamResult = ::CreateStreamOnHGlobal(nullptr, TRUE, getter_AddRefs(stream));
    if (FAILED(createStreamResult)) {
      return;
    }

    marshalResult = ::CoMarshalInterface(stream, aIID, aObject, MSHCTX_LOCAL,
                                         nullptr, mshlFlags);
    if (FAILED(marshalResult)) {
      return;
    }

    STATSTG statstg;
    statResult = stream->Stat(&statstg, STATFLAG_NONAME);
    if (SUCCEEDED(statResult)) {
      streamSize = static_cast<int>(statstg.cbSize.LowPart);
    } else {
      return;
    }

    getHGlobalResult = ::GetHGlobalFromStream(stream, &hglobal);
    MOZ_ASSERT(SUCCEEDED(getHGlobalResult));
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
  if (FAILED(createStreamResult)) {
    nsPrintfCString hrAsStr("0x%08X", createStreamResult);
    CrashReporter::AnnotateCrashReport(
        NS_LITERAL_CSTRING("CreateStreamOnHGlobalFailure"),
        hrAsStr);
  }

  if (FAILED(marshalResult)) {
    AnnotateInterfaceRegistration(aIID);
    nsPrintfCString hrAsStr("0x%08X", marshalResult);
    CrashReporter::AnnotateCrashReport(
        NS_LITERAL_CSTRING("CoMarshalInterfaceFailure"), hrAsStr);
  }

  if (FAILED(statResult)) {
    nsPrintfCString hrAsStr("0x%08X", statResult);
    CrashReporter::AnnotateCrashReport(
        NS_LITERAL_CSTRING("StatFailure"),
        hrAsStr);
  }

  if (FAILED(getHGlobalResult)) {
    nsPrintfCString hrAsStr("0x%08X", getHGlobalResult);
    CrashReporter::AnnotateCrashReport(
        NS_LITERAL_CSTRING("GetHGlobalFromStreamFailure"),
        hrAsStr);
  }
#endif // defined(MOZ_CRASHREPORTER)

  mStream = mozilla::Move(stream);

  if (streamSize) {
#if defined(MOZ_CRASHREPORTER)
    CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("ProxyStreamSizeFrom"),
                                       NS_LITERAL_CSTRING("IStream::Stat"));
#endif // defined(MOZ_CRASHREPORTER)
    mBufSize = streamSize;
  }

  if (!hglobal) {
    return;
  }

  mGlobalLockedBuf = reinterpret_cast<BYTE*>(::GlobalLock(hglobal));
  mHGlobal = hglobal;

  // If we couldn't get the stream size directly from mStream, we may use
  // the size of the memory block allocated by the HGLOBAL, though it might
  // be larger than the actual stream size.
  if (!streamSize) {
#if defined(MOZ_CRASHREPORTER)
    CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("ProxyStreamSizeFrom"),
                                       NS_LITERAL_CSTRING("GlobalSize"));
#endif // defined(MOZ_CRASHREPORTER)
    mBufSize = static_cast<int>(::GlobalSize(hglobal));
  }

#if defined(MOZ_CRASHREPORTER)
  nsAutoCString strBufSize;
  strBufSize.AppendInt(mBufSize);

  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("ProxyStreamSize"),
                                     strBufSize);
#endif // defined(MOZ_CRASHREPORTER)
}

} // namespace mscom
} // namespace mozilla

