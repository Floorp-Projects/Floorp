/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TextDecoderStream.h"

#include "nsContentUtils.h"
#include "nsIGlobalObject.h"
#include "mozilla/Encoding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/TextDecoderStreamBinding.h"
#include "mozilla/dom/TransformerCallbackHelpers.h"
#include "mozilla/dom/TransformStream.h"
#include "mozilla/dom/UnionTypes.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TextDecoderStream, mGlobal, mStream)
NS_IMPL_CYCLE_COLLECTING_ADDREF(TextDecoderStream)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TextDecoderStream)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TextDecoderStream)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

TextDecoderStream::TextDecoderStream(nsISupports* aGlobal,
                                     const Encoding& aEncoding, bool aFatal,
                                     bool aIgnoreBOM, TransformStream& aStream)
    : mGlobal(aGlobal), mStream(&aStream) {
  mFatal = aFatal;
  mIgnoreBOM = aIgnoreBOM;
  aEncoding.Name(mEncoding);
  if (aIgnoreBOM) {
    mDecoder = aEncoding.NewDecoderWithoutBOMHandling();
  } else {
    mDecoder = aEncoding.NewDecoderWithBOMRemoval();
  }
}

TextDecoderStream::~TextDecoderStream() = default;

JSObject* TextDecoderStream::WrapObject(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return TextDecoderStream_Binding::Wrap(aCx, this, aGivenProto);
}

class TextDecoderStreamAlgorithms : public TransformerAlgorithmsWrapper {
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TextDecoderStreamAlgorithms,
                                           TransformerAlgorithmsBase)

  void SetDecoderStream(TextDecoderStream& aStream) {
    mDecoderStream = &aStream;
  }

  // The common part of decode-and-enqueue and flush-and-enqueue.
  // Note that the most of the decoding algorithm is implemented in
  // mozilla::Decoder, and this is mainly about calling it properly.
  // https://encoding.spec.whatwg.org/#decode-and-enqueue-a-chunk
  // TODO: This does not allow shared array buffers, just as the non-stream
  // TextDecoder/Encoder don't. (Bug 1561594)
  MOZ_CAN_RUN_SCRIPT void DecodeBufferSourceAndEnqueue(
      JSContext* aCx, OwningArrayBufferViewOrArrayBuffer* aInput, bool aFlush,
      TransformStreamDefaultController& aController, ErrorResult& aRv) {
    nsString outDecodedString;
    if (aInput) {
      ProcessTypedArrays(*aInput, [&](const Span<const uint8_t>& aData,
                                      JS::AutoCheckCannotGC&&) {
        mDecoderStream->DecodeNative(aData, !aFlush, outDecodedString, aRv);
      });
    } else {
      mDecoderStream->DecodeNative(Span<const uint8_t>(), !aFlush,
                                   outDecodedString, aRv);
    }

    if (aRv.Failed()) {
      return;
    }

    if (outDecodedString.Length()) {
      // Step 4.2. If outputChunk is non-empty, then enqueue outputChunk in
      // decoder’s transform.
      JS::Rooted<JS::Value> outputChunk(aCx);
      if (!ToJSValue(aCx, outDecodedString, &outputChunk)) {
        JS_ClearPendingException(aCx);
        aRv.Throw(NS_ERROR_UNEXPECTED);
        return;
      }
      aController.Enqueue(aCx, outputChunk, aRv);
    }
  }

  // https://encoding.spec.whatwg.org/#dom-textdecoderstream
  MOZ_CAN_RUN_SCRIPT void TransformCallbackImpl(
      JS::Handle<JS::Value> aChunk,
      TransformStreamDefaultController& aController,
      ErrorResult& aRv) override {
    // Step 7. Let transformAlgorithm be an algorithm which takes a chunk
    // argument and runs the decode and enqueue a chunk algorithm with this and
    // chunk.

    // https://encoding.spec.whatwg.org/#decode-and-enqueue-a-chunk

    AutoJSAPI jsapi;
    if (!jsapi.Init(aController.GetParentObject())) {
      aRv.ThrowUnknownError("Internal error");
      return;
    }
    JSContext* cx = jsapi.cx();

    // Step 1. Let bufferSource be the result of converting chunk to an
    // [AllowShared] BufferSource.
    RootedUnion<OwningArrayBufferViewOrArrayBuffer> bufferSource(cx);
    if (!bufferSource.Init(cx, aChunk)) {
      aRv.MightThrowJSException();
      aRv.StealExceptionFromJSContext(cx);
      return;
    }

    DecodeBufferSourceAndEnqueue(cx, &bufferSource, false, aController, aRv);
  }

  // https://encoding.spec.whatwg.org/#dom-textdecoderstream
  MOZ_CAN_RUN_SCRIPT void FlushCallbackImpl(
      TransformStreamDefaultController& aController,
      ErrorResult& aRv) override {
    // Step 8. Let flushAlgorithm be an algorithm which takes no arguments and
    // runs the flush and enqueue algorithm with this.

    AutoJSAPI jsapi;
    if (!jsapi.Init(aController.GetParentObject())) {
      aRv.ThrowUnknownError("Internal error");
      return;
    }
    JSContext* cx = jsapi.cx();

    // https://encoding.spec.whatwg.org/#flush-and-enqueue
    // (The flush and enqueue algorithm is basically a subset of decode and
    // enqueue one, so let's reuse it)
    DecodeBufferSourceAndEnqueue(cx, nullptr, true, aController, aRv);
  }

 private:
  ~TextDecoderStreamAlgorithms() override = default;

  RefPtr<TextDecoderStream> mDecoderStream;
};

NS_IMPL_CYCLE_COLLECTION_INHERITED(TextDecoderStreamAlgorithms,
                                   TransformerAlgorithmsBase, mDecoderStream)
NS_IMPL_ADDREF_INHERITED(TextDecoderStreamAlgorithms, TransformerAlgorithmsBase)
NS_IMPL_RELEASE_INHERITED(TextDecoderStreamAlgorithms,
                          TransformerAlgorithmsBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TextDecoderStreamAlgorithms)
NS_INTERFACE_MAP_END_INHERITING(TransformerAlgorithmsBase)

// https://encoding.spec.whatwg.org/#dom-textdecoderstream
already_AddRefed<TextDecoderStream> TextDecoderStream::Constructor(
    const GlobalObject& aGlobal, const nsAString& aLabel,
    const TextDecoderOptions& aOptions, ErrorResult& aRv) {
  // Step 1. Let encoding be the result of getting an encoding from label.
  const Encoding* encoding = Encoding::ForLabelNoReplacement(aLabel);

  // Step 2. If encoding is failure or replacement, then throw a RangeError
  if (!encoding) {
    NS_ConvertUTF16toUTF8 label(aLabel);
    label.Trim(" \t\n\f\r");
    aRv.ThrowRangeError<MSG_ENCODING_NOT_SUPPORTED>(label);
    return nullptr;
  }

  // Step 3-6. (Done in the constructor)

  // Step 7-8.
  auto algorithms = MakeRefPtr<TextDecoderStreamAlgorithms>();

  // Step 9-10.
  RefPtr<TransformStream> transformStream =
      TransformStream::CreateGeneric(aGlobal, *algorithms, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 11. (Done in the constructor)
  auto decoderStream = MakeRefPtr<TextDecoderStream>(
      aGlobal.GetAsSupports(), *encoding, aOptions.mFatal, aOptions.mIgnoreBOM,
      *transformStream);
  algorithms->SetDecoderStream(*decoderStream);
  return decoderStream.forget();
}

ReadableStream* TextDecoderStream::Readable() const {
  return mStream->Readable();
}

WritableStream* TextDecoderStream::Writable() const {
  return mStream->Writable();
}

}  // namespace mozilla::dom
