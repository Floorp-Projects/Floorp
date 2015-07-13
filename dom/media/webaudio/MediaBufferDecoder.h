/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaBufferDecoder_h_
#define MediaBufferDecoder_h_

#include "nsWrapperCache.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/MemoryReporting.h"

namespace mozilla {

namespace dom {
class AudioBuffer;
class AudioContext;
class DecodeErrorCallback;
class DecodeSuccessCallback;
class Promise;
} // namespace dom

struct WebAudioDecodeJob final
{
  // You may omit both the success and failure callback, or you must pass both.
  // The callbacks are only necessary for asynchronous operation.
  WebAudioDecodeJob(const nsACString& aContentType,
                    dom::AudioContext* aContext,
                    dom::Promise* aPromise,
                    dom::DecodeSuccessCallback* aSuccessCallback = nullptr,
                    dom::DecodeErrorCallback* aFailureCallback = nullptr);

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebAudioDecodeJob)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebAudioDecodeJob)

  enum ErrorCode {
    NoError,
    UnknownContent,
    UnknownError,
    InvalidContent,
    NoAudio
  };

  typedef void (WebAudioDecodeJob::*ResultFn)(ErrorCode);
  typedef nsAutoArrayPtr<float> ChannelBuffer;

  void OnSuccess(ErrorCode /* ignored */);
  void OnFailure(ErrorCode aErrorCode);

  bool AllocateBuffer();

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  nsCString mContentType;
  uint32_t mWriteIndex;
  nsRefPtr<dom::AudioContext> mContext;
  nsRefPtr<dom::Promise> mPromise;
  nsRefPtr<dom::DecodeSuccessCallback> mSuccessCallback;
  nsRefPtr<dom::DecodeErrorCallback> mFailureCallback; // can be null
  nsRefPtr<dom::AudioBuffer> mOutput;
  FallibleTArray<ChannelBuffer> mChannelBuffers;

private:
  ~WebAudioDecodeJob();
};

void AsyncDecodeWebAudio(const char* aContentType, uint8_t* aBuffer,
                         uint32_t aLength, WebAudioDecodeJob& aDecodeJob);

} // namespace mozilla

#endif

