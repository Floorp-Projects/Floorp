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

struct IStream;
struct IUnknown;

namespace mozilla {
namespace mscom {

bool IsCOMInitializedOnCurrentThread();
bool IsCurrentThreadMTA();
bool IsCurrentThreadExplicitMTA();
bool IsCurrentThreadImplicitMTA();
bool IsProxy(IUnknown* aUnknown);
bool IsValidGUID(REFGUID aCheckGuid);
uintptr_t GetContainingModuleHandle();

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
uint32_t CreateStream(const uint8_t* aBuf, const uint32_t aBufLen,
                      IStream** aOutStream);

/**
 * Creates a deep copy of a proxy contained in a stream.
 * @param aInStream Stream containing the proxy to copy. Its seek pointer must
 *                  be positioned to point at the beginning of the proxy data.
 * @param aOutStream Outparam to receive the newly created stream.
 * @return HRESULT error code.
 */
uint32_t CopySerializedProxy(IStream* aInStream, IStream** aOutStream);

#if defined(MOZILLA_INTERNAL_API)
void GUIDToString(REFGUID aGuid, nsAString& aOutString);
#endif  // defined(MOZILLA_INTERNAL_API)

#if defined(ACCESSIBILITY)
bool IsVtableIndexFromParentInterface(REFIID aInterface,
                                      unsigned long aVtableIndex);

#  if defined(MOZILLA_INTERNAL_API)
bool IsCallerExternalProcess();

bool IsInterfaceEqualToOrInheritedFrom(REFIID aInterface, REFIID aFrom,
                                       unsigned long aVtableIndexHint);
#  endif  // defined(MOZILLA_INTERNAL_API)

#endif  // defined(ACCESSIBILITY)

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
