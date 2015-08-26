/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FuzzingWrapper.h"

PRLogModuleInfo* GetFuzzingWrapperLog() {
  static PRLogModuleInfo* log = nullptr;
  if (!log) {
    log = PR_NewLogModule("MediaFuzzingWrapper");
  }
  return log;
}
#define DFW_LOGD(arg, ...) MOZ_LOG(GetFuzzingWrapperLog(), mozilla::LogLevel::Debug, ("DecoderFuzzingWrapper(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#define DFW_LOGV(arg, ...) MOZ_LOG(GetFuzzingWrapperLog(), mozilla::LogLevel::Verbose, ("DecoderFuzzingWrapper(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#define CFW_LOGD(arg, ...) MOZ_LOG(GetFuzzingWrapperLog(), mozilla::LogLevel::Debug, ("DecoderCallbackFuzzingWrapper(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#define CFW_LOGV(arg, ...) MOZ_LOG(GetFuzzingWrapperLog(), mozilla::LogLevel::Verbose, ("DecoderCallbackFuzzingWrapper(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

namespace mozilla {

DecoderFuzzingWrapper::DecoderFuzzingWrapper(
    already_AddRefed<MediaDataDecoder> aDecoder,
    already_AddRefed<DecoderCallbackFuzzingWrapper> aCallbackWrapper)
  : mDecoder(aDecoder)
  , mCallbackWrapper(aCallbackWrapper)
{
  DFW_LOGV("aDecoder=%p aCallbackWrapper=%p", mDecoder.get(), mCallbackWrapper.get());
}

DecoderFuzzingWrapper::~DecoderFuzzingWrapper()
{
  DFW_LOGV("");
}

nsRefPtr<MediaDataDecoder::InitPromise>
DecoderFuzzingWrapper::Init()
{
  DFW_LOGV("");
  MOZ_ASSERT(mDecoder);
  return mDecoder->Init();
}

nsresult
DecoderFuzzingWrapper::Input(MediaRawData* aData)
{
  DFW_LOGV("aData.mTime=%lld", aData->mTime);
  MOZ_ASSERT(mDecoder);
  return mDecoder->Input(aData);
}

nsresult
DecoderFuzzingWrapper::Flush()
{
  DFW_LOGV("");
  MOZ_ASSERT(mDecoder);
  return mDecoder->Flush();
}

nsresult
DecoderFuzzingWrapper::Drain()
{
  DFW_LOGV("");
  MOZ_ASSERT(mDecoder);
  return mDecoder->Drain();
}

nsresult
DecoderFuzzingWrapper::Shutdown()
{
  DFW_LOGV("");
  MOZ_ASSERT(mDecoder);
  return mDecoder->Shutdown();
}

bool
DecoderFuzzingWrapper::IsHardwareAccelerated(nsACString& aFailureReason) const
{
  DFW_LOGV("");
  MOZ_ASSERT(mDecoder);
  return mDecoder->IsHardwareAccelerated(aFailureReason);
}

nsresult
DecoderFuzzingWrapper::ConfigurationChanged(const TrackInfo& aConfig)
{
  DFW_LOGV("");
  MOZ_ASSERT(mDecoder);
  return mDecoder->ConfigurationChanged(aConfig);
}


DecoderCallbackFuzzingWrapper::DecoderCallbackFuzzingWrapper(MediaDataDecoderCallback* aCallback)
  : mCallback(aCallback)
{
  CFW_LOGV("aCallback=%p", aCallback);
}

DecoderCallbackFuzzingWrapper::~DecoderCallbackFuzzingWrapper()
{
  CFW_LOGV("");
}

void
DecoderCallbackFuzzingWrapper::Output(MediaData* aData)
{
  CFW_LOGV("aData.mTime=%lld", aData->mTime);
  MOZ_ASSERT(mCallback);
  mCallback->Output(aData);
}

void
DecoderCallbackFuzzingWrapper::Error()
{
  CFW_LOGV("");
  MOZ_ASSERT(mCallback);
  mCallback->Error();
}

void
DecoderCallbackFuzzingWrapper::InputExhausted()
{
  CFW_LOGV("");
  MOZ_ASSERT(mCallback);
  mCallback->InputExhausted();
}

void
DecoderCallbackFuzzingWrapper::DrainComplete()
{
  CFW_LOGV("");
  MOZ_ASSERT(mCallback);
  mCallback->DrainComplete();
}

void
DecoderCallbackFuzzingWrapper::ReleaseMediaResources()
{
  CFW_LOGV("");
  MOZ_ASSERT(mCallback);
  mCallback->ReleaseMediaResources();
}

bool
DecoderCallbackFuzzingWrapper::OnReaderTaskQueue()
{
  CFW_LOGV("");
  MOZ_ASSERT(mCallback);
  return mCallback->OnReaderTaskQueue();
}

} // namespace mozilla
