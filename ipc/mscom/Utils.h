/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_Utils_h
#define mozilla_mscom_Utils_h

#if defined(MOZILLA_INTERNAL_API)
#  include "nsString.h"
#endif  // defined(MOZILLA_INTERNAL_API)

#include "mozilla/Attributes.h"
#include <guiddef.h>
#include <stdint.h>

struct IStream;
struct IUnknown;

namespace mozilla {
namespace mscom {
namespace detail {

enum class GuidType {
  CLSID,
  AppID,
};

long BuildRegGuidPath(REFGUID aGuid, const GuidType aGuidType, wchar_t* aBuf,
                      const size_t aBufLen);

}  // namespace detail

bool IsCOMInitializedOnCurrentThread();
bool IsCurrentThreadMTA();
bool IsCurrentThreadExplicitMTA();
bool IsCurrentThreadImplicitMTA();
#if defined(MOZILLA_INTERNAL_API)
bool IsCurrentThreadNonMainMTA();
#endif  // defined(MOZILLA_INTERNAL_API)
bool IsProxy(IUnknown* aUnknown);
bool IsValidGUID(REFGUID aCheckGuid);
uintptr_t GetContainingModuleHandle();

template <size_t N>
inline long BuildAppidPath(REFGUID aAppId, wchar_t (&aPath)[N]) {
  return detail::BuildRegGuidPath(aAppId, detail::GuidType::AppID, aPath, N);
}

template <size_t N>
inline long BuildClsidPath(REFCLSID aClsid, wchar_t (&aPath)[N]) {
  return detail::BuildRegGuidPath(aClsid, detail::GuidType::CLSID, aPath, N);
}

/**
 * Given a buffer, create a new IStream object.
 * @param aBuf Buffer containing data to initialize the stream. This parameter
 *             may be nullptr, causing the stream to be created with aBufLen
 *             bytes of uninitialized data.
 * @param aBufLen Length of data in aBuf, or desired stream size if aBuf is
 *                nullptr.
 * @param aOutStream Outparam to receive the newly created stream.
 * @return HRESULT error code.
 */
long CreateStream(const uint8_t* aBuf, const uint32_t aBufLen,
                  IStream** aOutStream);

/**
 * Length of a stringified GUID as formatted for the registry, i.e. including
 * curly-braces and dashes.
 */
constexpr size_t kGuidRegFormatCharLenInclNul = 39;

#if defined(MOZILLA_INTERNAL_API)
/**
 * Checks the registry to see if |aClsid| is a thread-aware in-process server.
 *
 * In DCOM, an in-process server is a server that is implemented inside a DLL
 * that is loaded into the client's process for execution. If |aClsid| declares
 * itself to be a local server (that is, a server that resides in another
 * process), this function returns false.
 *
 * For the server to be thread-aware, its registry entry must declare a
 * ThreadingModel that is one of "Free", "Both", or "Neutral". If the threading
 * model is "Apartment" or some other, invalid value, the class is treated as
 * being single-threaded.
 *
 * NB: This function cannot check CLSIDs that were registered via manifests,
 * as unfortunately there is not a documented API available to query for those.
 * This should not be an issue for most CLSIDs that Gecko is interested in, as
 * we typically instantiate system CLSIDs which are available in the registry.
 *
 * @param aClsid The CLSID of the COM class to be checked.
 * @return true if the class meets the above criteria, otherwise false.
 */
bool IsClassThreadAwareInprocServer(REFCLSID aClsid);

void GUIDToString(REFGUID aGuid, nsAString& aOutString);

/**
 * Converts an IID to a human-readable string for the purposes of diagnostic
 * tools such as the profiler. For some special cases, we output a friendly
 * string that describes the purpose of the interface. If no such description
 * exists, we simply fall back to outputting the IID as a string formatted by
 * GUIDToString().
 */
void DiagnosticNameForIID(REFIID aIid, nsACString& aOutString);
#else
void GUIDToString(REFGUID aGuid,
                  wchar_t (&aOutBuf)[kGuidRegFormatCharLenInclNul]);
#endif  // defined(MOZILLA_INTERNAL_API)

/**
 * Execute cleanup code when going out of scope if a condition is met.
 * This is useful when, for example, particular cleanup needs to be performed
 * whenever a call returns a failure HRESULT.
 * Both the condition and cleanup code are provided as functions (usually
 * lambdas).
 */
template <typename CondFnT, typename ExeFnT>
class MOZ_RAII ExecuteWhen final {
 public:
  ExecuteWhen(CondFnT& aCondFn, ExeFnT& aExeFn)
      : mCondFn(aCondFn), mExeFn(aExeFn) {}

  ~ExecuteWhen() {
    if (mCondFn()) {
      mExeFn();
    }
  }

  ExecuteWhen(const ExecuteWhen&) = delete;
  ExecuteWhen(ExecuteWhen&&) = delete;
  ExecuteWhen& operator=(const ExecuteWhen&) = delete;
  ExecuteWhen& operator=(ExecuteWhen&&) = delete;

 private:
  CondFnT& mCondFn;
  ExeFnT& mExeFn;
};

}  // namespace mscom
}  // namespace mozilla

#endif  // mozilla_mscom_Utils_h
