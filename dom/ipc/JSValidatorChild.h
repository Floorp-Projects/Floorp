/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_JSValidatorChild
#define mozilla_dom_JSValidatorChild

#include "mozilla/Result.h"
#include "mozilla/ProcInfo.h"
#include "mozilla/dom/PJSValidatorChild.h"
#include "mozilla/dom/JSValidatorUtils.h"
#include "mozilla/net/OpaqueResponseUtils.h"

template <typename T>
using BufferUniquePtr = mozilla::UniquePtr<T, JS::FreePolicy>;

namespace mozilla {
class Decoder;
}  // namespace mozilla

namespace mozilla::dom {
class JSValidatorChild final : public PJSValidatorChild {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(JSValidatorChild, override);

  mozilla::ipc::IPCResult RecvIsOpaqueResponseAllowed(
      IsOpaqueResponseAllowedResolver&& aResolver);

  mozilla::ipc::IPCResult RecvOnDataAvailable(Shmem&& aData);

  mozilla::ipc::IPCResult RecvOnStopRequest(const nsresult& aReason,
                                            const nsACString& aContentCharset,
                                            const nsAString& aHintCharset,
                                            const nsAString& aDocumentCharset);

  void ActorDestroy(ActorDestroyReason aReason) override;

 private:
  virtual ~JSValidatorChild() = default;

  using ValidatorResult = net::OpaqueResponseBlocker::ValidatorResult;
  void Resolve(ValidatorResult aResult);
  ValidatorResult ShouldAllowJS(const mozilla::Span<const char>& aSpan) const;

  mozilla::Result<mozilla::Span<const char>, nsresult> GetUTF8EncodedContent(
      const mozilla::Span<const uint8_t>& aData,
      BufferUniquePtr<Utf8Unit[]>& aBuffer,
      UniquePtr<mozilla::Decoder>& aDecoder);

  nsCString mSourceBytes;
  Maybe<IsOpaqueResponseAllowedResolver> mResolver;
};
}  // namespace mozilla::dom

#endif  // defined(mozilla_dom_JSValidatorChild)
