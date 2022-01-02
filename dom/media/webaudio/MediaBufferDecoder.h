/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaBufferDecoder_h_
#define MediaBufferDecoder_h_

#include "AudioSegment.h"
#include "nsWrapperCache.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/MemoryReporting.h"

namespace mozilla {

class ThreadSharedFloatArrayBufferList;

namespace dom {
class AudioBuffer;
class AudioContext;
class DecodeErrorCallback;
class DecodeSuccessCallback;
class Promise;
}  // namespace dom

struct WebAudioDecodeJob final {
  // You may omit both the success and failure callback, or you must pass both.
  // The callbacks are only necessary for asynchronous operation.
  WebAudioDecodeJob(dom::AudioContext* aContext, dom::Promise* aPromise,
                    dom::DecodeSuccessCallback* aSuccessCallback = nullptr,
                    dom::DecodeErrorCallback* aFailureCallback = nullptr);
  ~WebAudioDecodeJob();

  enum ErrorCode {
    NoError,
    UnknownContent,
    UnknownError,
    InvalidContent,
    NoAudio
  };

  typedef void (WebAudioDecodeJob::*ResultFn)(ErrorCode);

  MOZ_CAN_RUN_SCRIPT void OnSuccess(ErrorCode /* ignored */);
  MOZ_CAN_RUN_SCRIPT void OnFailure(ErrorCode aErrorCode);

  bool AllocateBuffer();

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  AudioChunk mBuffer;
  RefPtr<dom::AudioContext> mContext;
  RefPtr<dom::Promise> mPromise;
  RefPtr<dom::DecodeSuccessCallback> mSuccessCallback;
  RefPtr<dom::DecodeErrorCallback> mFailureCallback;  // can be null
  RefPtr<dom::AudioBuffer> mOutput;
};

void AsyncDecodeWebAudio(const char* aContentType, uint8_t* aBuffer,
                         uint32_t aLength, WebAudioDecodeJob& aDecodeJob);

}  // namespace mozilla

#endif
