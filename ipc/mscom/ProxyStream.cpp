/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <utility>
#if defined(ACCESSIBILITY)
#  include "HandlerData.h"
#  include "mozilla/a11y/Platform.h"
#  include "mozilla/mscom/ActivationContext.h"
#endif  // defined(ACCESSIBILITY)
#include "mozilla/mscom/EnsureMTA.h"
#include "mozilla/mscom/ProxyStream.h"
#include "mozilla/mscom/Utils.h"
#include "mozilla/ScopeExit.h"

#include "mozilla/mscom/Objref.h"
#include "nsExceptionHandler.h"
#include "nsPrintfCString.h"
#include "RegistrationAnnotator.h"

#include <windows.h>
#include <objbase.h>
#include <shlwapi.h>

namespace mozilla {
namespace mscom {

ProxyStream::ProxyStream()
    : mGlobalLockedBuf(nullptr),
      mHGlobal(nullptr),
      mBufSize(0),
      mPreserveStream(false) {}

// GetBuffer() fails with this variant, but that's okay because we're just
// reconstructing the stream from a buffer anyway.
ProxyStream::ProxyStream(REFIID aIID, const BYTE* aInitBuf,
                         const int aInitBufSize, Environment* aEnv)
    : mGlobalLockedBuf(nullptr),
      mHGlobal(nullptr),
      mBufSize(aInitBufSize),
      mPreserveStream(false) {
  CrashReporter::Annotation kCrashReportKey =
      CrashReporter::Annotation::ProxyStreamUnmarshalStatus;

  if (!aInitBufSize) {
    CrashReporter::AnnotateCrashReport(kCrashReportKey, "!aInitBufSize"_ns);
    // We marshaled a nullptr. Nothing else to do here.
    return;
  }

  HRESULT createStreamResult =
      CreateStream(aInitBuf, aInitBufSize, getter_AddRefs(mStream));
  if (FAILED(createStreamResult)) {
    nsPrintfCString hrAsStr("0x%08X", createStreamResult);
    CrashReporter::AnnotateCrashReport(kCrashReportKey, hrAsStr);
    return;
  }

  // NB: We can't check for a null mStream until after we have checked for
  // the zero aInitBufSize above. This is because InitStream will also fail
  // in that case, even though marshaling a nullptr is allowable.
  MOZ_ASSERT(mStream);
  if (!mStream) {
    CrashReporter::AnnotateCrashReport(kCrashReportKey, "!mStream"_ns);
    return;
  }

#if defined(ACCESSIBILITY)
  const uint32_t expectedStreamLen = GetOBJREFSize(WrapNotNull(mStream));
  nsAutoCString strActCtx;
  nsAutoString manifestPath;
#endif  // defined(ACCESSIBILITY)

  HRESULT unmarshalResult = S_OK;

  // We need to convert to an interface here otherwise we mess up const
  // correctness with IPDL. We'll request an IUnknown and then QI the
  // actual interface later.

#if defined(ACCESSIBILITY)
  auto marshalFn = [this, &strActCtx, &manifestPath, &unmarshalResult, &aIID,
                    aEnv]() -> void
#else
  auto marshalFn = [this, &unmarshalResult, &aIID, aEnv]() -> void
#endif  // defined(ACCESSIBILITY)
  {
    if (aEnv) {
      bool pushOk = aEnv->Push();
      MOZ_DIAGNOSTIC_ASSERT(pushOk);
      if (!pushOk) {
        return;
      }
    }

    auto popEnv = MakeScopeExit([aEnv]() -> void {
      if (!aEnv) {
        return;
      }

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
      bool popOk =
#endif
          aEnv->Pop();
      MOZ_DIAGNOSTIC_ASSERT(popOk);
    });

#if defined(ACCESSIBILITY)
    auto curActCtx = ActivationContext::GetCurrent();
    if (curActCtx.isOk()) {
      strActCtx.AppendPrintf("0x%p", curActCtx.unwrap());
    } else {
      strActCtx.AppendPrintf("HRESULT 0x%08X", curActCtx.unwrapErr());
    }

    ActivationContext::GetCurrentManifestPath(manifestPath);
#endif  // defined(ACCESSIBILITY)

    unmarshalResult = ::CoUnmarshalInterface(mStream, aIID,
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

  mStream = nullptr;

  if (FAILED(unmarshalResult) || !mUnmarshaledProxy) {
    nsPrintfCString hrAsStr("0x%08X", unmarshalResult);
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::CoUnmarshalInterfaceResult, hrAsStr);
    AnnotateInterfaceRegistration(aIID);
    if (!mUnmarshaledProxy) {
      CrashReporter::AnnotateCrashReport(kCrashReportKey,
                                         "!mUnmarshaledProxy"_ns);
    }

#if defined(ACCESSIBILITY)
    AnnotateClassRegistration(CLSID_AccessibleHandler);
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::UnmarshalActCtx, strActCtx);
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::UnmarshalActCtxManifestPath,
        NS_ConvertUTF16toUTF8(manifestPath));
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::A11yHandlerRegistered,
        a11y::IsHandlerRegistered() ? "true"_ns : "false"_ns);

    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::ExpectedStreamLen, expectedStreamLen);

    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::ActualStreamLen, aInitBufSize);
#endif  // defined(ACCESSIBILITY)
  }
}

ProxyStream::ProxyStream(ProxyStream&& aOther)
    : mGlobalLockedBuf(nullptr),
      mHGlobal(nullptr),
      mBufSize(0),
      mPreserveStream(false) {
  *this = std::move(aOther);
}

ProxyStream& ProxyStream::operator=(ProxyStream&& aOther) {
  if (mHGlobal && mGlobalLockedBuf) {
    DebugOnly<BOOL> result = ::GlobalUnlock(mHGlobal);
    MOZ_ASSERT(!result && ::GetLastError() == NO_ERROR);
  }

  mStream = std::move(aOther.mStream);

  mGlobalLockedBuf = aOther.mGlobalLockedBuf;
  aOther.mGlobalLockedBuf = nullptr;

  // ::GlobalFree() was called implicitly when mStream was replaced.
  mHGlobal = aOther.mHGlobal;
  aOther.mHGlobal = nullptr;

  mBufSize = aOther.mBufSize;
  aOther.mBufSize = 0;

  mUnmarshaledProxy = std::move(aOther.mUnmarshaledProxy);

  mPreserveStream = aOther.mPreserveStream;
  return *this;
}

ProxyStream::~ProxyStream() {
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

const BYTE* ProxyStream::GetBuffer(int& aReturnedBufSize) const {
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

PreservedStreamPtr ProxyStream::GetPreservedStream() {
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
  return ToPreservedStreamPtr(std::move(cloned));
}

bool ProxyStream::GetInterface(void** aOutInterface) {
  // We should not have a locked buffer on this side
  MOZ_ASSERT(!mGlobalLockedBuf);
  MOZ_ASSERT(aOutInterface);

  if (!aOutInterface) {
    return false;
  }

  *aOutInterface = mUnmarshaledProxy.release();
  return true;
}

ProxyStream::ProxyStream(REFIID aIID, IUnknown* aObject, Environment* aEnv,
                         ProxyStreamFlags aFlags)
    : mGlobalLockedBuf(nullptr),
      mHGlobal(nullptr),
      mBufSize(0),
      mPreserveStream(aFlags & ProxyStreamFlags::ePreservable) {
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

#if defined(ACCESSIBILITY)
  nsAutoString manifestPath;
  auto marshalFn = [&aIID, aObject, mshlFlags, &stream, &streamSize, &hglobal,
                    &createStreamResult, &marshalResult, &statResult,
                    &getHGlobalResult, aEnv, &manifestPath]() -> void {
#else
  auto marshalFn = [&aIID, aObject, mshlFlags, &stream, &streamSize, &hglobal,
                    &createStreamResult, &marshalResult, &statResult,
                    &getHGlobalResult, aEnv]() -> void {
#endif  // defined(ACCESSIBILITY)
    if (aEnv) {
      bool pushOk = aEnv->Push();
      MOZ_DIAGNOSTIC_ASSERT(pushOk);
      if (!pushOk) {
        return;
      }
    }

    auto popEnv = MakeScopeExit([aEnv]() -> void {
      if (!aEnv) {
        return;
      }

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
      bool popOk =
#endif
          aEnv->Pop();
      MOZ_DIAGNOSTIC_ASSERT(popOk);
    });

    createStreamResult =
        ::CreateStreamOnHGlobal(nullptr, TRUE, getter_AddRefs(stream));
    if (FAILED(createStreamResult)) {
      return;
    }

#if defined(ACCESSIBILITY)
    ActivationContext::GetCurrentManifestPath(manifestPath);
#endif  // defined(ACCESSIBILITY)

    marshalResult = ::CoMarshalInterface(stream, aIID, aObject, MSHCTX_LOCAL,
                                         nullptr, mshlFlags);
    MOZ_ASSERT(marshalResult != E_INVALIDARG);
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

  if (FAILED(createStreamResult)) {
    nsPrintfCString hrAsStr("0x%08X", createStreamResult);
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::CreateStreamOnHGlobalFailure, hrAsStr);
  }

  if (FAILED(marshalResult)) {
    AnnotateInterfaceRegistration(aIID);
    nsPrintfCString hrAsStr("0x%08X", marshalResult);
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::CoMarshalInterfaceFailure, hrAsStr);
#if defined(ACCESSIBILITY)
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::MarshalActCtxManifestPath,
        NS_ConvertUTF16toUTF8(manifestPath));
#endif  // defined(ACCESSIBILITY)
  }

  if (FAILED(statResult)) {
    nsPrintfCString hrAsStr("0x%08X", statResult);
    CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::StatFailure,
                                       hrAsStr);
  }

  if (FAILED(getHGlobalResult)) {
    nsPrintfCString hrAsStr("0x%08X", getHGlobalResult);
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::GetHGlobalFromStreamFailure, hrAsStr);
  }

  mStream = std::move(stream);

  if (streamSize) {
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::ProxyStreamSizeFrom, "IStream::Stat"_ns);
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
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::ProxyStreamSizeFrom, "GlobalSize"_ns);
    mBufSize = static_cast<int>(::GlobalSize(hglobal));
  }

  CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::ProxyStreamSize,
                                     mBufSize);
}

}  // namespace mscom
}  // namespace mozilla
