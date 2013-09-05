/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaRecorder_h
#define MediaRecorder_h

#include "mozilla/dom/MediaRecorderBinding.h"
#include "nsDOMEventTargetHelper.h"

// Max size for allowing queue encoded data in memory
#define MAX_ALLOW_MEMORY_BUFFER 1024000
namespace mozilla {

class ErrorResult;
class DOMMediaStream;
class EncodedBufferCache;
class MediaEncoder;
class ProcessedMediaStream;
class MediaInputPort;

namespace dom {

/**
 * Implementation of https://dvcs.w3.org/hg/dap/raw-file/default/media-stream-capture/MediaRecorder.html
 * The MediaRecorder accepts a mediaStream as input source passed from UA. When recorder starts,
 * a MediaEncoder will be created and accept the mediaStream as input source.
 * Encoder will get the raw data by track data changes, encode it by selected MIME Type, then store the encoded in EncodedBufferCache object.
 * The encoded data will be extracted on every timeslice passed from Start function call or by RequestData function.
 * Thread model:
 * When the recorder starts, it creates a "Media Encoder" thread to read data from MediaEncoder object and store buffer in EncodedBufferCache object.
 * Also extract the encoded data and create blobs on every timeslice passed from start function or RequestData function called by UA.
 */

class MediaRecorder : public nsDOMEventTargetHelper
{
  class ExtractEncodedDataTask;
  class PushBlobTask;
  class PushErrorMessageTask;
public:
  MediaRecorder(DOMMediaStream&);
  virtual ~MediaRecorder();

  // nsWrapperCache
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  nsPIDOMWindow* GetParentObject() { return GetOwner(); }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MediaRecorder,
                                           nsDOMEventTargetHelper)

  // WebIDL
  // Start recording. If timeSlice has been provided, mediaRecorder will
  // raise a dataavailable event containing the Blob of collected data on every timeSlice milliseconds.
  // If timeSlice isn't provided, UA should call the RequestData to obtain the Blob data, also set the mTimeSlice to zero.
  void Start(const Optional<int32_t>& timeSlice, ErrorResult & aResult);
  // Stop the recording activiy. Including stop the Media Encoder thread, un-hook the mediaStreamListener to encoder.
  void Stop(ErrorResult& aResult);
  // Pause the mTrackUnionStream
  void Pause(ErrorResult& aResult);

  void Resume(ErrorResult& aResult);
  // Extract encoded data Blob from EncodedBufferCache.
  void RequestData(ErrorResult& aResult);
  // Return the The DOMMediaStream passed from UA.
  DOMMediaStream* Stream() const { return mStream; }
  // The current state of the MediaRecorder object.
  RecordingState State() const { return mState; }
  // Return the current encoding MIME type selected by the MediaEncoder.
  void GetMimeType(nsString &aMimeType) { aMimeType = mMimeType; }

  static already_AddRefed<MediaRecorder>
  Constructor(const GlobalObject& aGlobal,
              DOMMediaStream& aStream, ErrorResult& aRv);

  // EventHandler
  IMPL_EVENT_HANDLER(dataavailable)
  IMPL_EVENT_HANDLER(error)
  IMPL_EVENT_HANDLER(stop)
  IMPL_EVENT_HANDLER(warning)

  friend class ExtractEncodedData;

protected:
  void Init(nsPIDOMWindow* aOwnerWindow);
  // Copy encoded data from encoder to EncodedBufferCache. This function runs in the Media Encoder Thread.
  void ExtractEncodedData();

  MediaRecorder& operator = (const MediaRecorder& x) MOZ_DELETE;
  // Create dataavailable event with Blob data and it runs in main thread
  nsresult CreateAndDispatchBlobEvent();
  // Creating a simple event to notify UA simple event.
  void DispatchSimpleEvent(const nsAString & aStr);
  // Creating a error event with message.
  void NotifyError(nsresult aRv);
  // Check if the recorder's principal is the subsume of mediaStream
  bool CheckPrincipal();

  MediaRecorder(const MediaRecorder& x) MOZ_DELETE; // prevent bad usage


  // Runnable thread for read data from mediaEncoder. It starts at MediaRecorder::Start() and stops at MediaRecorder::Stop().
  nsCOMPtr<nsIThread> mReadThread;
  // The MediaEncoder object initializes on start() and destroys in ~MediaRecorder.
  nsRefPtr<MediaEncoder> mEncoder;
  // MediaStream passed from js context
  nsRefPtr<DOMMediaStream> mStream;
  // This media stream is used for notifying raw data to encoder and can be blocked.
  nsRefPtr<ProcessedMediaStream> mTrackUnionStream;
  // This is used for destroing the inputport when destroy the mediaRecorder
  nsRefPtr<MediaInputPort> mStreamPort;
  // This object creates on start() and destroys in ~MediaRecorder.
  nsAutoPtr<EncodedBufferCache> mEncodedBufferCache;
  // It specifies the container format as well as the audio and video capture formats.
  nsString mMimeType;
  // The interval of timer passed from Start(). On every mTimeSlice milliseconds, if there are buffers store in the EncodedBufferCache,
  // a dataavailable event will be fired.
  int32_t mTimeSlice;
  // The current state of the MediaRecorder object.
  RecordingState mState;
};

}
}

#endif
