/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/JSValidatorChild.h"
#include "js/JSON.h"
#include "mozilla/dom/JSOracleChild.h"

#include "mozilla/Encoding.h"
#include "mozilla/dom/ScriptDecoding.h"
#include "mozilla/ipc/Endpoint.h"

#include "js/experimental/JSStencil.h"
#include "js/SourceText.h"
#include "js/Exception.h"
#include "js/GlobalObject.h"
#include "js/CompileOptions.h"
#include "js/RealmOptions.h"

using namespace mozilla::dom;
using Encoding = mozilla::Encoding;

mozilla::UniquePtr<mozilla::Decoder> TryGetDecoder(
    const mozilla::Span<const uint8_t>& aSourceBytes,
    const nsACString& aContentCharset, const nsAString& aHintCharset,
    const nsAString& aDocumentCharset) {
  const Encoding* encoding;
  mozilla::UniquePtr<mozilla::Decoder> unicodeDecoder;

  std::tie(encoding, std::ignore) = Encoding::ForBOM(aSourceBytes);
  if (encoding) {
    unicodeDecoder = encoding->NewDecoderWithBOMRemoval();
  }

  if (!unicodeDecoder) {
    encoding = Encoding::ForLabel(aContentCharset);
    if (encoding) {
      unicodeDecoder = encoding->NewDecoderWithoutBOMHandling();
    }

    if (!unicodeDecoder) {
      encoding = Encoding::ForLabel(aHintCharset);
      if (encoding) {
        unicodeDecoder = encoding->NewDecoderWithoutBOMHandling();
      }
    }

    if (!unicodeDecoder) {
      encoding = Encoding::ForLabel(aDocumentCharset);
      if (encoding) {
        unicodeDecoder = encoding->NewDecoderWithoutBOMHandling();
      }
    }
  }

  if (!unicodeDecoder && !IsUtf8(mozilla::Span(reinterpret_cast<const char*>(
                                                   aSourceBytes.Elements()),
                                               aSourceBytes.Length()))) {
    // Curiously, there are various callers that don't pass aDocument. The
    // fallback in the old code was ISO-8859-1, which behaved like
    // windows-1252.
    unicodeDecoder = WINDOWS_1252_ENCODING->NewDecoderWithoutBOMHandling();
  }

  return unicodeDecoder;
}

mozilla::ipc::IPCResult JSValidatorChild::RecvIsOpaqueResponseAllowed(
    IsOpaqueResponseAllowedResolver&& aResolver) {
  mResolver.emplace(aResolver);

  return IPC_OK();
}

mozilla::ipc::IPCResult JSValidatorChild::RecvOnDataAvailable(Shmem&& aData) {
  if (!mResolver) {
    MOZ_ASSERT(!CanSend());
    return IPC_OK();
  }

  if (!mSourceBytes.Append(Span(aData.get<char>(), aData.Size<char>()),
                           mozilla::fallible)) {
    // To prevent an attacker from flood the validation process,
    // we don't validate here.
    Resolve(ValidatorResult::Failure);
  }
  DeallocShmem(aData);

  return IPC_OK();
}

mozilla::ipc::IPCResult JSValidatorChild::RecvOnStopRequest(
    const nsresult& aReason, const nsACString& aContentCharset,
    const nsAString& aHintCharset, const nsAString& aDocumentCharset) {
  if (!mResolver) {
    return IPC_OK();
  }

  if (NS_FAILED(aReason)) {
    Resolve(ValidatorResult::Failure);
  } else if (mSourceBytes.IsEmpty()) {
    // The empty document parses as JavaScript.
    Resolve(ValidatorResult::JavaScript);
  } else {
    UniquePtr<Decoder> unicodeDecoder = TryGetDecoder(
        mSourceBytes, aContentCharset, aHintCharset, aDocumentCharset);

    if (!unicodeDecoder) {
      Resolve(ShouldAllowJS(mSourceBytes));
    } else {
      BufferUniquePtr<Utf8Unit[]> buffer;
      auto result = GetUTF8EncodedContent(mSourceBytes, buffer, unicodeDecoder);
      if (result.isErr()) {
        Resolve(ValidatorResult::Failure);
      } else {
        Resolve(ShouldAllowJS(result.unwrap()));
      }
    }
  }

  return IPC_OK();
}

void JSValidatorChild::ActorDestroy(ActorDestroyReason aReason) {
  if (mResolver) {
    Resolve(ValidatorResult::Failure);
  }
};

void JSValidatorChild::Resolve(ValidatorResult aResult) {
  MOZ_ASSERT(mResolver);
  Maybe<Shmem> data = Nothing();
  if (aResult == ValidatorResult::JavaScript && !mSourceBytes.IsEmpty()) {
    Shmem sharedData;
    nsresult rv =
        JSValidatorUtils::CopyCStringToShmem(this, mSourceBytes, sharedData);
    if (NS_SUCCEEDED(rv)) {
      data = Some(std::move(sharedData));
    }
  }

  mResolver.ref()(std::tuple<mozilla::Maybe<Shmem>&&, const ValidatorResult&>(
      std::move(data), aResult));
  mResolver.reset();
}

mozilla::Result<mozilla::Span<const char>, nsresult>
JSValidatorChild::GetUTF8EncodedContent(
    const mozilla::Span<const uint8_t>& aData,
    BufferUniquePtr<Utf8Unit[]>& aBuffer, UniquePtr<Decoder>& aDecoder) {
  MOZ_ASSERT(aDecoder);
  // We need the output buffer to be UTF8
  CheckedInt<size_t> bufferLength =
      ScriptDecoding<Utf8Unit>::MaxBufferLength(aDecoder, aData.Length());
  if (!bufferLength.isValid()) {
    return mozilla::Err(NS_ERROR_FAILURE);
  }

  CheckedInt<size_t> bufferByteSize = bufferLength * sizeof(Utf8Unit);
  if (!bufferByteSize.isValid()) {
    return mozilla::Err(NS_ERROR_FAILURE);
  }

  aBuffer.reset(static_cast<Utf8Unit*>(js_malloc(bufferByteSize.value())));
  if (!aBuffer) {
    return mozilla::Err(NS_ERROR_FAILURE);
  }

  size_t written = ScriptDecoding<Utf8Unit>::DecodeInto(
      aDecoder, aData, Span(aBuffer.get(), bufferLength.value()),
      /* aEndOfSource = */ true);
  MOZ_ASSERT(written <= bufferLength.value());
  MOZ_ASSERT(
      IsUtf8(Span(reinterpret_cast<const char*>(aBuffer.get()), written)));

  return Span(reinterpret_cast<const char*>(aBuffer.get()), written);
}

JSValidatorChild::ValidatorResult JSValidatorChild::ShouldAllowJS(
    const mozilla::Span<const char>& aSpan) const {
  MOZ_ASSERT(!aSpan.IsEmpty());

  MOZ_DIAGNOSTIC_ASSERT(IsUtf8(aSpan));

  JSContext* cx = JSOracleChild::JSContext();
  if (!cx) {
    return ValidatorResult::Failure;
  }

  JS::Rooted<JSObject*> global(cx, JSOracleChild::JSObject());
  if (!global) {
    return ValidatorResult::Failure;
  }

  JS::SourceText<Utf8Unit> srcBuf;
  if (!srcBuf.init(cx, aSpan.Elements(), aSpan.Length(),
                   JS::SourceOwnership::Borrowed)) {
    JS_ClearPendingException(cx);
    return ValidatorResult::Failure;
  }

  JSAutoRealm ar(cx, global);

  // Parse to JavaScript
  RefPtr<JS::Stencil> stencil =
      CompileGlobalScriptToStencil(cx, JS::CompileOptions(cx), srcBuf);

  if (!stencil) {
    JS_ClearPendingException(cx);
    return ValidatorResult::Other;
  }

  MOZ_ASSERT(!aSpan.IsEmpty());

  // Parse to JSON
  JS::Rooted<JS::Value> json(cx);
  if (IsAscii(aSpan)) {
    // Ascii is a subset of Latin1, and JS_ParseJSON can take Latin1 directly
    if (JS_ParseJSON(cx,
                     reinterpret_cast<const JS::Latin1Char*>(aSpan.Elements()),
                     aSpan.Length(), &json)) {
      return ValidatorResult::JSON;
    }
  } else {
    nsString decoded;
    nsresult rv = UTF_8_ENCODING->DecodeWithBOMRemoval(
        Span(reinterpret_cast<const uint8_t*>(aSpan.Elements()),
             aSpan.Length()),
        decoded);
    if (NS_FAILED(rv)) {
      return ValidatorResult::Failure;
    }

    if (JS_ParseJSON(cx, decoded.BeginReading(), decoded.Length(), &json)) {
      return ValidatorResult::JSON;
    }
  }

  // Since the JSON parsing failed, we confirmed the file is Javascript and not
  // JSON.
  if (JS_IsExceptionPending(cx)) {
    JS_ClearPendingException(cx);
  }
  return ValidatorResult::JavaScript;
}
