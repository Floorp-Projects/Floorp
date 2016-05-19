/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsThreadUtils.h"
#include "nsXPCOMCIDInternal.h"
#include "PocketSphinxSpeechRecognitionService.h"
#include "nsIFile.h"
#include "SpeechGrammar.h"
#include "SpeechRecognition.h"
#include "SpeechRecognitionAlternative.h"
#include "SpeechRecognitionResult.h"
#include "SpeechRecognitionResultList.h"
#include "nsIObserverService.h"
#include "MediaPrefs.h"
#include "mozilla/Services.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsMemory.h"

extern "C" {
#include "pocketsphinx/pocketsphinx.h"
#include "sphinxbase/logmath.h"
#include "sphinxbase/sphinx_config.h"
#include "sphinxbase/jsgf.h"
}

namespace mozilla {

using namespace dom;

class DecodeResultTask : public Runnable
{
public:
  DecodeResultTask(const nsString& hypstring,
                   float64 confidence,
                   WeakPtr<dom::SpeechRecognition> recognition)
      : mResult(hypstring),
        mConfidence(confidence),
        mRecognition(recognition),
        mWorkerThread(do_GetCurrentThread())
  {
    MOZ_ASSERT(
      !NS_IsMainThread()); // This should be running on the worker thread
  }

  NS_IMETHOD
  Run()
  {
    MOZ_ASSERT(NS_IsMainThread()); // This method is supposed to run on the main
                                   // thread!

    // Declare javascript result events
    RefPtr<SpeechEvent> event = new SpeechEvent(
      mRecognition, SpeechRecognition::EVENT_RECOGNITIONSERVICE_FINAL_RESULT);
    SpeechRecognitionResultList* resultList =
      new SpeechRecognitionResultList(mRecognition);
    SpeechRecognitionResult* result = new SpeechRecognitionResult(mRecognition);
    if (0 < mRecognition->MaxAlternatives()) {
      SpeechRecognitionAlternative* alternative =
        new SpeechRecognitionAlternative(mRecognition);

      alternative->mTranscript = mResult;
      alternative->mConfidence = mConfidence;

      result->mItems.AppendElement(alternative);
    }
    resultList->mItems.AppendElement(result);

    event->mRecognitionResultList = resultList;
    NS_DispatchToMainThread(event);

    // If we don't destroy the thread when we're done with it, it will hang
    // around forever... bad!
    // But thread->Shutdown must be called from the main thread, not from the
    // thread itself.
    return mWorkerThread->Shutdown();
  }

private:
  nsString mResult;
  float64 mConfidence;
  WeakPtr<dom::SpeechRecognition> mRecognition;
  nsCOMPtr<nsIThread> mWorkerThread;
};

class DecodeTask : public Runnable
{
public:
  DecodeTask(WeakPtr<dom::SpeechRecognition> recogntion,
             const nsTArray<int16_t>& audiovector, ps_decoder_t* ps)
      : mRecognition(recogntion), mAudiovector(audiovector), mPs(ps)
  {
  }

  NS_IMETHOD
  Run()
  {
    char const* hyp;
    int rv;
    int32 final;
    int32 logprob;
    float64 confidence;
    nsAutoCString hypoValue;

    rv = ps_start_utt(mPs);
    rv = ps_process_raw(mPs, &mAudiovector[0], mAudiovector.Length(), FALSE,
                        FALSE);

    rv = ps_end_utt(mPs);
    confidence = 0;
    if (rv >= 0) {
      hyp = ps_get_hyp_final(mPs, &final);
      if (hyp && final) {
        logprob = ps_get_prob(mPs);
        confidence = logmath_exp(ps_get_logmath(mPs), logprob);
        hypoValue.Assign(hyp);
      }
    }

    nsCOMPtr<nsIRunnable> resultrunnable =
      new DecodeResultTask(NS_ConvertUTF8toUTF16(hypoValue), confidence, mRecognition);
    return NS_DispatchToMainThread(resultrunnable);
  }

private:
  WeakPtr<dom::SpeechRecognition> mRecognition;
  nsTArray<int16_t> mAudiovector;
  ps_decoder_t* mPs;
};

NS_IMPL_ISUPPORTS(PocketSphinxSpeechRecognitionService,
                  nsISpeechRecognitionService, nsIObserver)

PocketSphinxSpeechRecognitionService::PocketSphinxSpeechRecognitionService()
{
  mSpeexState = nullptr;

  // get root folder
  nsCOMPtr<nsIFile> tmpFile;
  nsAutoString aStringAMPath;   // am folder
  nsAutoString aStringDictPath; // dict folder

  NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(tmpFile));
#if defined(XP_WIN) // for some reason, on windows NS_GRE_DIR is not bin root,
                    // but bin/browser
  tmpFile->AppendRelativePath(NS_LITERAL_STRING(".."));
#endif
  tmpFile->AppendRelativePath(NS_LITERAL_STRING("models"));
  tmpFile->AppendRelativePath(NS_LITERAL_STRING("en-US"));
  tmpFile->GetPath(aStringAMPath);

  NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(tmpFile));
#if defined(XP_WIN) // for some reason, on windows NS_GRE_DIR is not bin root,
                    // but bin/browser
  tmpFile->AppendRelativePath(NS_LITERAL_STRING(".."));
#endif
  tmpFile->AppendRelativePath(NS_LITERAL_STRING("models"));     //
  tmpFile->AppendRelativePath(NS_LITERAL_STRING("dict"));       //
  tmpFile->AppendRelativePath(NS_LITERAL_STRING("en-US.dic")); //
  tmpFile->GetPath(aStringDictPath);

  // FOR B2G PATHS HARDCODED (APPEND /DATA ON THE BEGINING, FOR DESKTOP, ONLY
  // MODELS/ RELATIVE TO ROOT
  mPSConfig = cmd_ln_init(nullptr, ps_args(), TRUE, "-bestpath", "yes", "-hmm",
                          ToNewUTF8String(aStringAMPath), // acoustic model
                          "-dict", ToNewUTF8String(aStringDictPath), nullptr);
  if (mPSConfig == nullptr) {
    ISDecoderCreated = false;
  } else {
    mPSHandle = ps_init(mPSConfig);
    if (mPSHandle == nullptr) {
      ISDecoderCreated = false;
    } else {
      ISDecoderCreated = true;
    }
  }

  ISGrammarCompiled = false;
}

PocketSphinxSpeechRecognitionService::~PocketSphinxSpeechRecognitionService()
{
  if (mPSConfig) {
    free(mPSConfig);
  }
  if (mPSHandle) {
    free(mPSHandle);
  }

  mSpeexState = nullptr;
}

// CALL START IN JS FALLS HERE
NS_IMETHODIMP
PocketSphinxSpeechRecognitionService::Initialize(
    WeakPtr<SpeechRecognition> aSpeechRecognition)
{
  if (!ISDecoderCreated || !ISGrammarCompiled) {
    return NS_ERROR_NOT_INITIALIZED;
  } else {
    mAudioVector.Clear();

    if (mSpeexState) {
      mSpeexState = nullptr;
    }

    mRecognition = aSpeechRecognition;
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    obs->AddObserver(this, SPEECH_RECOGNITION_TEST_EVENT_REQUEST_TOPIC, false);
    obs->AddObserver(this, SPEECH_RECOGNITION_TEST_END_TOPIC, false);
    return NS_OK;
  }
}

NS_IMETHODIMP
PocketSphinxSpeechRecognitionService::ProcessAudioSegment(
  AudioSegment* aAudioSegment, int32_t aSampleRate)
{
  if (!mSpeexState) {
    mSpeexState = speex_resampler_init(1, aSampleRate, 16000,
                                       SPEEX_RESAMPLER_QUALITY_MAX, nullptr);
  }
  aAudioSegment->ResampleChunks(mSpeexState, aSampleRate, 16000);

  AudioSegment::ChunkIterator iterator(*aAudioSegment);

  while (!iterator.IsEnded()) {
    mozilla::AudioChunk& chunk = *(iterator);
    MOZ_ASSERT(chunk.mBuffer);
    const int16_t* buf = static_cast<const int16_t*>(chunk.mChannelData[0]);

    for (int i = 0; i < iterator->mDuration; i++) {
      mAudioVector.AppendElement((int16_t)buf[i]);
    }
    iterator.Next();
  }
  return NS_OK;
}

NS_IMETHODIMP
PocketSphinxSpeechRecognitionService::SoundEnd()
{
  speex_resampler_destroy(mSpeexState);
  mSpeexState = nullptr;

  // To create a new thread, get the thread manager
  nsCOMPtr<nsIThreadManager> tm = do_GetService(NS_THREADMANAGER_CONTRACTID);
  nsCOMPtr<nsIThread> decodethread;
  nsresult rv = tm->NewThread(0, 0, getter_AddRefs(decodethread));
  if (NS_FAILED(rv)) {
    // In case of failure, call back immediately with an empty string which
    // indicates failure
    return NS_OK;
  }

  nsCOMPtr<nsIRunnable> r =
    new DecodeTask(mRecognition, mAudioVector, mPSHandle);
  decodethread->Dispatch(r, nsIEventTarget::DISPATCH_NORMAL);

  return NS_OK;
}

NS_IMETHODIMP
PocketSphinxSpeechRecognitionService::ValidateAndSetGrammarList(
  SpeechGrammar* aSpeechGrammar,
  nsISpeechGrammarCompilationCallback* aCallback)
{
  if (!ISDecoderCreated) {
    ISGrammarCompiled = false;
  } else if (aSpeechGrammar) {
    nsAutoString grammar;
    ErrorResult rv;
    aSpeechGrammar->GetSrc(grammar, rv);

    int result = ps_set_jsgf_string(mPSHandle, "name",
                                    NS_ConvertUTF16toUTF8(grammar).get());

    if (result != 0) {
      ISGrammarCompiled = false;
    } else {
      ps_set_search(mPSHandle, "name");

      ISGrammarCompiled = true;
    }
  } else {
    ISGrammarCompiled = false;
  }

  return ISGrammarCompiled ? NS_OK : NS_ERROR_NOT_INITIALIZED;
}

NS_IMETHODIMP
PocketSphinxSpeechRecognitionService::Abort()
{
  return NS_OK;
}

NS_IMETHODIMP
PocketSphinxSpeechRecognitionService::Observe(nsISupports* aSubject,
                                              const char* aTopic,
                                              const char16_t* aData)
{
  MOZ_ASSERT(MediaPrefs::WebSpeechFakeRecognitionService(),
             "Got request to fake recognition service event, "
             "but " TEST_PREFERENCE_FAKE_RECOGNITION_SERVICE " is not set");

  if (!strcmp(aTopic, SPEECH_RECOGNITION_TEST_END_TOPIC)) {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    obs->RemoveObserver(this, SPEECH_RECOGNITION_TEST_EVENT_REQUEST_TOPIC);
    obs->RemoveObserver(this, SPEECH_RECOGNITION_TEST_END_TOPIC);

    return NS_OK;
  }

  const nsDependentString eventName = nsDependentString(aData);

  if (eventName.EqualsLiteral("EVENT_RECOGNITIONSERVICE_ERROR")) {
    mRecognition->DispatchError(
      SpeechRecognition::EVENT_RECOGNITIONSERVICE_ERROR,
      SpeechRecognitionErrorCode::Network, // TODO different codes?
      NS_LITERAL_STRING("RECOGNITIONSERVICE_ERROR test event"));

  } else if (eventName.EqualsLiteral("EVENT_RECOGNITIONSERVICE_FINAL_RESULT")) {
    RefPtr<SpeechEvent> event = new SpeechEvent(
      mRecognition, SpeechRecognition::EVENT_RECOGNITIONSERVICE_FINAL_RESULT);

    event->mRecognitionResultList = BuildMockResultList();
    NS_DispatchToMainThread(event);
  }

  return NS_OK;
}

SpeechRecognitionResultList*
PocketSphinxSpeechRecognitionService::BuildMockResultList()
{
  SpeechRecognitionResultList* resultList =
    new SpeechRecognitionResultList(mRecognition);
  SpeechRecognitionResult* result = new SpeechRecognitionResult(mRecognition);
  if (0 < mRecognition->MaxAlternatives()) {
    SpeechRecognitionAlternative* alternative =
      new SpeechRecognitionAlternative(mRecognition);

    alternative->mTranscript = NS_LITERAL_STRING("Mock final result");
    alternative->mConfidence = 0.0f;

    result->mItems.AppendElement(alternative);
  }
  resultList->mItems.AppendElement(result);

  return resultList;
}

} // namespace mozilla
