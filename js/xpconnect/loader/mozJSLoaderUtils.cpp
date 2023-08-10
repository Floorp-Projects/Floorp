/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/scache/StartupCache.h"

#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/CompileOptions.h"
#include "js/Transcoding.h"
#include "js/experimental/JSStencil.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/Span.h"

using namespace JS;
using namespace mozilla::scache;
using mozilla::UniquePtr;

static nsresult HandleTranscodeResult(JSContext* cx,
                                      JS::TranscodeResult result) {
  if (result == JS::TranscodeResult::Ok) {
    return NS_OK;
  }

  if (result == JS::TranscodeResult::Throw) {
    JS_ClearPendingException(cx);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  MOZ_ASSERT(IsTranscodeFailureResult(result));
  return NS_ERROR_FAILURE;
}

nsresult ReadCachedStencil(StartupCache* cache, nsACString& cachePath,
                           JSContext* cx,
                           const JS::ReadOnlyDecodeOptions& options,
                           JS::Stencil** stencilOut) {
  MOZ_ASSERT(options.borrowBuffer);
  MOZ_ASSERT(!options.usePinnedBytecode);

  const char* buf;
  uint32_t len;
  nsresult rv =
      cache->GetBuffer(PromiseFlatCString(cachePath).get(), &buf, &len);
  if (NS_FAILED(rv)) {
    return rv;  // don't warn since NOT_AVAILABLE is an ok error
  }

  JS::TranscodeRange range(AsBytes(mozilla::Span(buf, len)));
  JS::TranscodeResult code = JS::DecodeStencil(cx, options, range, stencilOut);
  return HandleTranscodeResult(cx, code);
}

nsresult WriteCachedStencil(StartupCache* cache, nsACString& cachePath,
                            JSContext* cx, JS::Stencil* stencil) {
  JS::TranscodeBuffer buffer;
  JS::TranscodeResult code = JS::EncodeStencil(cx, stencil, buffer);
  if (code != JS::TranscodeResult::Ok) {
    return HandleTranscodeResult(cx, code);
  }

  size_t size = buffer.length();
  if (size > UINT32_MAX) {
    return NS_ERROR_FAILURE;
  }

  // Move the vector buffer into a unique pointer buffer.
  mozilla::UniqueFreePtr<char[]> buf(
      reinterpret_cast<char*>(buffer.extractOrCopyRawBuffer()));
  nsresult rv = cache->PutBuffer(PromiseFlatCString(cachePath).get(),
                                 std::move(buf), size);
  return rv;
}
