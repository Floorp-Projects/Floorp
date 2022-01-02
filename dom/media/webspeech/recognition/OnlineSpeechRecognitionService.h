/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_OnlineRecognitionService_h
#define mozilla_dom_OnlineRecognitionService_h

#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsISpeechRecognitionService.h"
#include "speex/speex_resampler.h"
#include "nsIStreamListener.h"
#include "OpusTrackEncoder.h"
#include "ContainerWriter.h"

#define NS_ONLINE_SPEECH_RECOGNITION_SERVICE_CID \
  {0x0ff5ce56,                                   \
   0x5b09,                                       \
   0x4db8,                                       \
   {0xad, 0xc6, 0x82, 0x66, 0xaf, 0x95, 0xf8, 0x64}};

namespace mozilla {

namespace ipc {
class PrincipalInfo;
}  // namespace ipc

/**
 * Online implementation of the nsISpeechRecognitionService interface
 */
class OnlineSpeechRecognitionService : public nsISpeechRecognitionService,
                                       public nsIStreamListener {
 public:
  // Add XPCOM glue code
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISPEECHRECOGNITIONSERVICE
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  /**
   * Listener responsible for handling the events raised by the TrackEncoder
   */
  class SpeechEncoderListener : public TrackEncoderListener {
   public:
    explicit SpeechEncoderListener(OnlineSpeechRecognitionService* aService)
        : mService(aService), mOwningThread(AbstractThread::GetCurrent()) {}

    void Started(TrackEncoder* aEncoder) override {}

    void Initialized(TrackEncoder* aEncoder) override {
      MOZ_ASSERT(mOwningThread->IsCurrentThreadIn());
      mService->EncoderInitialized();
    }

    void Error(TrackEncoder* aEncoder) override {
      MOZ_ASSERT(mOwningThread->IsCurrentThreadIn());
      mService->EncoderError();
    }

   private:
    const RefPtr<OnlineSpeechRecognitionService> mService;
    const RefPtr<AbstractThread> mOwningThread;
  };

  /**
   * Default constructs a OnlineSpeechRecognitionService
   */
  OnlineSpeechRecognitionService();

  /**
   * Called by SpeechEncoderListener when the AudioTrackEncoder has been
   * initialized.
   */
  void EncoderInitialized();

  /**
   * Called after the AudioTrackEncoder has encoded all data for us to wrap in a
   * container and pass along.
   */
  void EncoderFinished();

  /**
   * Called by SpeechEncoderListener when the AudioTrackEncoder has
   * encountered an error.
   */
  void EncoderError();

 private:
  /**
   * Private destructor to prevent bypassing of reference counting
   */
  virtual ~OnlineSpeechRecognitionService();

  /** The associated SpeechRecognition */
  nsMainThreadPtrHandle<dom::SpeechRecognition> mRecognition;

  /**
   * Builds a mock SpeechRecognitionResultList
   */
  dom::SpeechRecognitionResultList* BuildMockResultList();

  /**
   * Method responsible for uploading the audio to the remote endpoint
   */
  void DoSTT();

  // Encoded and packaged ogg audio data
  nsTArray<nsTArray<uint8_t>> mEncodedData;
  // Member responsible for holding a reference to the TrackEncoderListener
  RefPtr<SpeechEncoderListener> mSpeechEncoderListener;
  // MediaQueue fed encoded data by mAudioEncoder
  MediaQueue<EncodedFrame> mEncodedAudioQueue;
  // Encoder responsible for encoding the frames from pcm to opus which is the
  // format supported by our backend
  UniquePtr<AudioTrackEncoder> mAudioEncoder;
  // Object responsible for wrapping the opus frames into an ogg container
  UniquePtr<ContainerWriter> mWriter;
  // Member responsible for storing the json string returned by the endpoint
  nsCString mBuf;
  // Used to calculate a ceiling on the time spent listening.
  TimeStamp mFirstIteration;
  // flag responsible to control if the user choose to abort
  bool mAborted = false;
  //  reference to the audio encoder queue
  RefPtr<TaskQueue> mEncodeTaskQueue;
};

}  // namespace mozilla

#endif
