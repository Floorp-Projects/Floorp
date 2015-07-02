/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_MEDIASOURCEDECODER_H_
#define MOZILLA_MEDIASOURCEDECODER_H_

#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsError.h"
#include "MediaDecoder.h"
#include "MediaSourceReader.h"

class nsIStreamListener;

namespace mozilla {

class MediaResource;
class MediaDecoderStateMachine;
class SourceBufferDecoder;
class TrackBuffer;
enum MSRangeRemovalAction : uint8_t;
class MediaSourceDemuxer;

namespace dom {

class HTMLMediaElement;
class MediaSource;

} // namespace dom

class MediaSourceDecoder : public MediaDecoder
{
public:
  explicit MediaSourceDecoder(dom::HTMLMediaElement* aElement);

  virtual MediaDecoder* Clone() override;
  virtual MediaDecoderStateMachine* CreateStateMachine() override;
  virtual nsresult Load(nsIStreamListener**, MediaDecoder*) override;
  virtual media::TimeIntervals GetSeekable() override;
  media::TimeIntervals GetBuffered() override;

  // We can't do this in the constructor because we don't know what type of
  // media we're dealing with by that point.
  void NotifyDormantSupported(bool aSupported)
  {
    MOZ_ASSERT(NS_IsMainThread());
    mDormantSupported = aSupported;
  }

  virtual void Shutdown() override;

  static already_AddRefed<MediaResource> CreateResource(nsIPrincipal* aPrincipal = nullptr);

  void AttachMediaSource(dom::MediaSource* aMediaSource);
  void DetachMediaSource();

  already_AddRefed<SourceBufferDecoder> CreateSubDecoder(const nsACString& aType,
                                                         int64_t aTimestampOffset /* microseconds */);
  void AddTrackBuffer(TrackBuffer* aTrackBuffer);
  void RemoveTrackBuffer(TrackBuffer* aTrackBuffer);
  void OnTrackBufferConfigured(TrackBuffer* aTrackBuffer, const MediaInfo& aInfo);

  void Ended(bool aEnded);
  bool IsExpectingMoreData() override;

  // Return the duration of the video in seconds.
  virtual double GetDuration() override;

  void SetInitialDuration(int64_t aDuration);
  void SetMediaSourceDuration(double aDuration, MSRangeRemovalAction aAction);
  double GetMediaSourceDuration();

  // Called whenever a TrackBuffer has new data appended or a new decoder
  // initializes.  Safe to call from any thread.
  void NotifyTimeRangesChanged();

  // Indicates the point in time at which the reader should consider
  // registered TrackBuffers essential for initialization.
  void PrepareReaderInitialization();

#ifdef MOZ_EME
  virtual nsresult SetCDMProxy(CDMProxy* aProxy) override;
#endif

  MediaSourceReader* GetReader()
  {
    MOZ_ASSERT(!mIsUsingFormatReader);
    return static_cast<MediaSourceReader*>(mReader.get());
  }
  MediaSourceDemuxer* GetDemuxer()
  {
    return mDemuxer;
  }

  // Returns true if aReader is a currently active audio or video
  // reader in this decoders MediaSourceReader.
  bool IsActiveReader(MediaDecoderReader* aReader);

  // Returns a string describing the state of the MediaSource internal
  // buffered data. Used for debugging purposes.
  void GetMozDebugReaderData(nsAString& aString);

private:
  void DoSetMediaSourceDuration(double aDuration);

  // The owning MediaSource holds a strong reference to this decoder, and
  // calls Attach/DetachMediaSource on this decoder to set and clear
  // mMediaSource.
  dom::MediaSource* mMediaSource;
  nsRefPtr<MediaDecoderReader> mReader;
  bool mIsUsingFormatReader;
  nsRefPtr<MediaSourceDemuxer> mDemuxer;

  Atomic<bool> mEnded;
};

} // namespace mozilla

#endif /* MOZILLA_MEDIASOURCEDECODER_H_ */
