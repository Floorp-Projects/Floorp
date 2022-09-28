/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsThreadUtils.h"
#include "nsXPCOMCIDInternal.h"
#include "OnlineSpeechRecognitionService.h"
#include "nsIFile.h"
#include "SpeechGrammar.h"
#include "SpeechRecognition.h"
#include "SpeechRecognitionAlternative.h"
#include "SpeechRecognitionResult.h"
#include "SpeechRecognitionResultList.h"
#include "nsIObserverService.h"
#include "mozilla/dom/Document.h"
#include "mozilla/Preferences.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/Services.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsNetUtil.h"
#include "nsContentUtils.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIPrincipal.h"
#include "nsIStreamListener.h"
#include "nsIUploadChannel2.h"
#include "mozilla/dom/ClientIPCTypes.h"
#include "nsStringStream.h"
#include "nsIOutputStream.h"
#include "nsStreamUtils.h"
#include "OpusTrackEncoder.h"
#include "OggWriter.h"
#include "nsIClassOfService.h"
#include <json/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace mozilla {

using namespace dom;

#define PREFERENCE_DEFAULT_RECOGNITION_ENDPOINT \
  "media.webspeech.service.endpoint"
#define DEFAULT_RECOGNITION_ENDPOINT "https://speaktome-2.services.mozilla.com/"
#define MAX_LISTENING_TIME_MS 10000

NS_IMPL_ISUPPORTS(OnlineSpeechRecognitionService, nsISpeechRecognitionService,
                  nsIStreamListener)

NS_IMETHODIMP
OnlineSpeechRecognitionService::OnStartRequest(nsIRequest* aRequest) {
  MOZ_ASSERT(NS_IsMainThread());
  return NS_OK;
}

static nsresult AssignResponseToBuffer(nsIInputStream* aIn, void* aClosure,
                                       const char* aFromRawSegment,
                                       uint32_t aToOffset, uint32_t aCount,
                                       uint32_t* aWriteCount) {
  nsCString* buf = static_cast<nsCString*>(aClosure);
  buf->Append(aFromRawSegment, aCount);
  *aWriteCount = aCount;
  return NS_OK;
}

NS_IMETHODIMP
OnlineSpeechRecognitionService::OnDataAvailable(nsIRequest* aRequest,
                                                nsIInputStream* aInputStream,
                                                uint64_t aOffset,
                                                uint32_t aCount) {
  MOZ_ASSERT(NS_IsMainThread());
  nsresult rv;
  uint32_t readCount;
  rv = aInputStream->ReadSegments(AssignResponseToBuffer, &mBuf, aCount,
                                  &readCount);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

NS_IMETHODIMP
OnlineSpeechRecognitionService::OnStopRequest(nsIRequest* aRequest,
                                              nsresult aStatusCode) {
  MOZ_ASSERT(NS_IsMainThread());

  auto clearBuf = MakeScopeExit([&] { mBuf.Truncate(); });

  if (mAborted) {
    return NS_OK;
  }

  bool success;
  float confidence = 0;
  Json::Value root;
  Json::CharReaderBuilder builder;
  bool parsingSuccessful;
  nsAutoCString result;
  nsAutoCString hypoValue;
  nsAutoCString errorMsg;
  SpeechRecognitionErrorCode errorCode;

  SR_LOG("STT Result: %s", mBuf.get());

  if (NS_FAILED(aStatusCode)) {
    success = false;
    errorMsg.AssignLiteral("Error connecting to the service.");
    errorCode = SpeechRecognitionErrorCode::Network;
  } else {
    success = true;
    UniquePtr<Json::CharReader> const reader(builder.newCharReader());
    parsingSuccessful =
        reader->parse(mBuf.BeginReading(), mBuf.EndReading(), &root, nullptr);
    if (!parsingSuccessful) {
      // there's an internal server error
      success = false;
      errorMsg.AssignLiteral("Internal server error");
      errorCode = SpeechRecognitionErrorCode::Network;
    } else {
      result.Assign(root.get("status", "error").asString().c_str());
      if (result.EqualsLiteral("ok")) {
        // ok, we have a result
        if (!root["data"].empty()) {
          hypoValue.Assign(root["data"][0].get("text", "").asString().c_str());
          confidence = root["data"][0].get("confidence", "0").asFloat();
        } else {
          success = false;
          errorMsg.AssignLiteral("Error reading result data.");
          errorCode = SpeechRecognitionErrorCode::Network;
        }
      } else {
        success = false;
        errorMsg.Assign(root.get("message", "").asString().c_str());
        errorCode = SpeechRecognitionErrorCode::No_speech;
      }
    }
  }

  if (!success) {
    mRecognition->DispatchError(
        SpeechRecognition::EVENT_RECOGNITIONSERVICE_ERROR, errorCode, errorMsg);
  } else {
    // Declare javascript result events
    RefPtr<SpeechEvent> event = new SpeechEvent(
        mRecognition, SpeechRecognition::EVENT_RECOGNITIONSERVICE_FINAL_RESULT);
    SpeechRecognitionResultList* resultList =
        new SpeechRecognitionResultList(mRecognition);
    SpeechRecognitionResult* result = new SpeechRecognitionResult(mRecognition);

    if (mRecognition->MaxAlternatives() > 0) {
      SpeechRecognitionAlternative* alternative =
          new SpeechRecognitionAlternative(mRecognition);

      alternative->mTranscript = NS_ConvertUTF8toUTF16(hypoValue);
      alternative->mConfidence = confidence;

      result->mItems.AppendElement(alternative);
    }
    resultList->mItems.AppendElement(result);

    event->mRecognitionResultList = resultList;
    NS_DispatchToMainThread(event);
  }

  return NS_OK;
}

OnlineSpeechRecognitionService::OnlineSpeechRecognitionService() = default;
OnlineSpeechRecognitionService::~OnlineSpeechRecognitionService() = default;

NS_IMETHODIMP
OnlineSpeechRecognitionService::Initialize(
    WeakPtr<SpeechRecognition> aSpeechRecognition) {
  MOZ_ASSERT(NS_IsMainThread());
  mWriter = MakeUnique<OggWriter>();
  mRecognition = new nsMainThreadPtrHolder<SpeechRecognition>(
      "OnlineSpeechRecognitionService::mRecognition", aSpeechRecognition);
  mEncodeTaskQueue = mRecognition->GetTaskQueueForEncoding();
  MOZ_ASSERT(mEncodeTaskQueue);
  return NS_OK;
}

void OnlineSpeechRecognitionService::EncoderFinished() {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(mEncodedAudioQueue.IsFinished());

  while (RefPtr<EncodedFrame> frame = mEncodedAudioQueue.PopFront()) {
    AutoTArray<RefPtr<EncodedFrame>, 1> frames({frame});
    DebugOnly<nsresult> rv =
        mWriter->WriteEncodedTrack(frames, mEncodedAudioQueue.AtEndOfStream()
                                               ? ContainerWriter::END_OF_STREAM
                                               : 0);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  mWriter->GetContainerData(&mEncodedData, ContainerWriter::FLUSH_NEEDED);
  MOZ_ASSERT(mWriter->IsWritingComplete());

  NS_DispatchToMainThread(
      NewRunnableMethod("OnlineSpeechRecognitionService::DoSTT", this,
                        &OnlineSpeechRecognitionService::DoSTT));
}

void OnlineSpeechRecognitionService::EncoderInitialized() {
  MOZ_ASSERT(!NS_IsMainThread());
  AutoTArray<RefPtr<TrackMetadataBase>, 1> metadata;
  metadata.AppendElement(mAudioEncoder->GetMetadata());
  if (metadata[0]->GetKind() != TrackMetadataBase::METADATA_OPUS) {
    SR_LOG("wrong meta data type!");
    MOZ_ASSERT_UNREACHABLE();
  }

  nsresult rv = mWriter->SetMetadata(metadata);
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));

  rv = mWriter->GetContainerData(&mEncodedData, ContainerWriter::GET_HEADER);
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));

  Unused << rv;
}

void OnlineSpeechRecognitionService::EncoderError() {
  MOZ_ASSERT(!NS_IsMainThread());
  SR_LOG("Error encoding frames.");
  mEncodedData.Clear();
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "SpeechRecognition::DispatchError",
      [this, self = RefPtr<OnlineSpeechRecognitionService>(this)]() {
        if (!mRecognition) {
          return;
        }
        mRecognition->DispatchError(
            SpeechRecognition::EVENT_RECOGNITIONSERVICE_ERROR,
            SpeechRecognitionErrorCode::Audio_capture, "Encoder error");
      }));
}

NS_IMETHODIMP
OnlineSpeechRecognitionService::ProcessAudioSegment(AudioSegment* aAudioSegment,
                                                    int32_t aSampleRate) {
  MOZ_ASSERT(!NS_IsMainThread());
  int64_t duration = aAudioSegment->GetDuration();
  if (duration <= 0) {
    return NS_OK;
  }

  if (!mAudioEncoder) {
    mSpeechEncoderListener = new SpeechEncoderListener(this);
    mAudioEncoder =
        MakeUnique<OpusTrackEncoder>(aSampleRate, mEncodedAudioQueue);
    RefPtr<AbstractThread> mEncoderThread = AbstractThread::GetCurrent();
    mAudioEncoder->SetWorkerThread(mEncoderThread);
    mAudioEncoder->RegisterListener(mSpeechEncoderListener);
  }

  mAudioEncoder->AppendAudioSegment(std::move(*aAudioSegment));

  TimeStamp now = TimeStamp::Now();
  if (mFirstIteration.IsNull()) {
    mFirstIteration = now;
  }

  if ((now - mFirstIteration).ToMilliseconds() >= MAX_LISTENING_TIME_MS) {
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "SpeechRecognition::Stop",
        [this, self = RefPtr<OnlineSpeechRecognitionService>(this)]() {
          if (!mRecognition) {
            return;
          }
          mRecognition->Stop();
        }));

    return NS_OK;
  }

  return NS_OK;
}

void OnlineSpeechRecognitionService::DoSTT() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mAborted) {
    return;
  }

  nsresult rv;
  nsCOMPtr<nsIChannel> chan;
  nsCOMPtr<nsIURI> uri;
  nsAutoCString speechRecognitionEndpoint;
  nsAutoCString prefEndpoint;
  nsAutoString language;

  Preferences::GetCString(PREFERENCE_DEFAULT_RECOGNITION_ENDPOINT,
                          prefEndpoint);

  if (!prefEndpoint.IsEmpty()) {
    speechRecognitionEndpoint = prefEndpoint;
  } else {
    speechRecognitionEndpoint = DEFAULT_RECOGNITION_ENDPOINT;
  }

  rv = NS_NewURI(getter_AddRefs(uri), speechRecognitionEndpoint, nullptr,
                 nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mRecognition->DispatchError(
        SpeechRecognition::EVENT_RECOGNITIONSERVICE_ERROR,
        SpeechRecognitionErrorCode::Network, "Unknown URI");
    return;
  }

  nsSecurityFlags secFlags = nsILoadInfo::SEC_REQUIRE_CORS_INHERITS_SEC_CONTEXT;
  nsLoadFlags loadFlags =
      nsIRequest::LOAD_NORMAL | nsIChannel::LOAD_BYPASS_SERVICE_WORKER;
  nsContentPolicyType contentPolicy = nsIContentPolicy::TYPE_OTHER;

  nsPIDOMWindowInner* window = mRecognition->GetOwner();
  if (NS_WARN_IF(!window)) {
    mRecognition->DispatchError(
        SpeechRecognition::EVENT_RECOGNITIONSERVICE_ERROR,
        SpeechRecognitionErrorCode::Aborted, "No window");
    return;
  }

  Document* doc = window->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    mRecognition->DispatchError(
        SpeechRecognition::EVENT_RECOGNITIONSERVICE_ERROR,
        SpeechRecognitionErrorCode::Aborted, "No document");
  }
  rv = NS_NewChannel(getter_AddRefs(chan), uri, doc->NodePrincipal(), secFlags,
                     contentPolicy, nullptr, nullptr, nullptr, nullptr,
                     loadFlags);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mRecognition->DispatchError(
        SpeechRecognition::EVENT_RECOGNITIONSERVICE_ERROR,
        SpeechRecognitionErrorCode::Network, "Failed to open channel");
    return;
  }

  nsCOMPtr<nsIHttpChannel> httpChan = do_QueryInterface(chan);
  if (httpChan) {
    rv = httpChan->SetRequestMethod("POST"_ns);
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
  }

  if (httpChan) {
    mRecognition->GetLang(language);
    // Accept-Language-STT is a custom header of our backend server used to set
    // the language of the speech sample being submitted by the client
    rv = httpChan->SetRequestHeader("Accept-Language-STT"_ns,
                                    NS_ConvertUTF16toUTF8(language), false);
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
    // Tell the server to not store the transcription by default
    rv = httpChan->SetRequestHeader("Store-Transcription"_ns, "0"_ns, false);
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
    // Tell the server to not store the sample by default
    rv = httpChan->SetRequestHeader("Store-Sample"_ns, "0"_ns, false);
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
    // Set the product tag as teh web speech api
    rv = httpChan->SetRequestHeader("Product-Tag"_ns, "wsa"_ns, false);
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
  }

  nsCOMPtr<nsIClassOfService> cos(do_QueryInterface(chan));
  if (cos) {
    cos->AddClassFlags(nsIClassOfService::UrgentStart);
  }

  nsCOMPtr<nsIUploadChannel2> uploadChan = do_QueryInterface(chan);
  if (uploadChan) {
    nsCOMPtr<nsIInputStream> bodyStream;
    uint32_t length = 0;
    for (const nsTArray<uint8_t>& chunk : mEncodedData) {
      length += chunk.Length();
    }

    nsTArray<uint8_t> audio;
    if (!audio.SetCapacity(length, fallible)) {
      mRecognition->DispatchError(
          SpeechRecognition::EVENT_RECOGNITIONSERVICE_ERROR,
          SpeechRecognitionErrorCode::Audio_capture, "Allocation error");
      return;
    }

    for (const nsTArray<uint8_t>& chunk : mEncodedData) {
      audio.AppendElements(chunk);
    }

    mEncodedData.Clear();

    rv = NS_NewByteInputStream(getter_AddRefs(bodyStream), std::move(audio));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mRecognition->DispatchError(
          SpeechRecognition::EVENT_RECOGNITIONSERVICE_ERROR,
          SpeechRecognitionErrorCode::Network, "Failed to open stream");
      return;
    }
    if (bodyStream) {
      rv = uploadChan->ExplicitSetUploadStream(bodyStream, "audio/ogg"_ns,
                                               length, "POST"_ns, false);
      MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
    }
  }

  rv = chan->AsyncOpen(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mRecognition->DispatchError(
        SpeechRecognition::EVENT_RECOGNITIONSERVICE_ERROR,
        SpeechRecognitionErrorCode::Network, "Internal server error");
  }
}

NS_IMETHODIMP
OnlineSpeechRecognitionService::SoundEnd() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mEncodeTaskQueue) {
    // Not initialized
    return NS_OK;
  }

  nsresult rv = mEncodeTaskQueue->Dispatch(NS_NewRunnableFunction(
      "OnlineSpeechRecognitionService::SoundEnd",
      [this, self = RefPtr<OnlineSpeechRecognitionService>(this)]() {
        if (mAudioEncoder) {
          mAudioEncoder->NotifyEndOfStream();
          mAudioEncoder->UnregisterListener(mSpeechEncoderListener);
          mSpeechEncoderListener = nullptr;
          mAudioEncoder = nullptr;
          EncoderFinished();
        }
      }));
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
  Unused << rv;

  mEncodeTaskQueue = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
OnlineSpeechRecognitionService::ValidateAndSetGrammarList(
    SpeechGrammar* aSpeechGrammar,
    nsISpeechGrammarCompilationCallback* aCallback) {
  // This is an online LVCSR (STT) service,
  // so we don't need to set a grammar
  return NS_OK;
}

NS_IMETHODIMP
OnlineSpeechRecognitionService::Abort() {
  MOZ_ASSERT(NS_IsMainThread());
  if (mAborted) {
    return NS_OK;
  }
  mAborted = true;
  return SoundEnd();
}
}  // namespace mozilla
