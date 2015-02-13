/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_MEDIASOURCEDECODER_H_
#define MOZILLA_MEDIASOURCEDECODER_H_

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

namespace dom {

class HTMLMediaElement;
class MediaSource;

} // namespace dom

class MediaSourceDecoder : public MediaDecoder
{
public:
  explicit MediaSourceDecoder(dom::HTMLMediaElement* aElement);

  virtual MediaDecoder* Clone() MOZ_OVERRIDE;
  virtual MediaDecoderStateMachine* CreateStateMachine() MOZ_OVERRIDE;
  virtual nsresult Load(nsIStreamListener**, MediaDecoder*) MOZ_OVERRIDE;
  virtual nsresult GetSeekable(dom::TimeRanges* aSeekable) MOZ_OVERRIDE;

  virtual void Shutdown() MOZ_OVERRIDE;

  static already_AddRefed<MediaResource> CreateResource(nsIPrincipal* aPrincipal = nullptr);

  void AttachMediaSource(dom::MediaSource* aMediaSource);
  void DetachMediaSource();

  already_AddRefed<SourceBufferDecoder> CreateSubDecoder(const nsACString& aType,
                                                         int64_t aTimestampOffset /* microseconds */);
  void AddTrackBuffer(TrackBuffer* aTrackBuffer);
  void RemoveTrackBuffer(TrackBuffer* aTrackBuffer);
  void OnTrackBufferConfigured(TrackBuffer* aTrackBuffer, const MediaInfo& aInfo);

  void Ended();
  bool IsExpectingMoreData() MOZ_OVERRIDE;

  // Return the duration of the video in seconds.
  virtual double GetDuration() MOZ_OVERRIDE;

  void SetInitialDuration(int64_t aDuration);
  void SetMediaSourceDuration(double aDuration, MSRangeRemovalAction aAction);
  double GetMediaSourceDuration();
  void DurationChanged(double aOldDuration, double aNewDuration);

  // Called whenever a TrackBuffer has new data appended or a new decoder
  // initializes.  Safe to call from any thread.
  void NotifyTimeRangesChanged();

  // Indicates the point in time at which the reader should consider
  // registered TrackBuffers essential for initialization.
  void PrepareReaderInitialization();

#ifdef MOZ_EME
  virtual nsresult SetCDMProxy(CDMProxy* aProxy) MOZ_OVERRIDE;
#endif

  MediaSourceReader* GetReader() { return mReader; }

  // Returns true if aReader is a currently active audio or video
  // reader in this decoders MediaSourceReader.
  bool IsActiveReader(MediaDecoderReader* aReader);

  // Return a decoder from the set available in aTrackDecoders that has data
  // available in the range requested by aTarget.
  already_AddRefed<SourceBufferDecoder> SelectDecoder(int64_t aTarget /* microseconds */,
                                                      int64_t aTolerance /* microseconds */,
                                                      const nsTArray<nsRefPtr<SourceBufferDecoder>>& aTrackDecoders);

  // Returns a string describing the state of the MediaSource internal
  // buffered data. Used for debugging purposes.
  void GetMozDebugReaderData(nsAString& aString);

private:
  void DoSetMediaSourceDuration(double aDuration);
  void ScheduleDurationChange(double aOldDuration,
                              double aNewDuration,
                              MSRangeRemovalAction aAction);

  // The owning MediaSource holds a strong reference to this decoder, and
  // calls Attach/DetachMediaSource on this decoder to set and clear
  // mMediaSource.
  dom::MediaSource* mMediaSource;
  nsRefPtr<MediaSourceReader> mReader;

  // Protected by GetReentrantMonitor()
  double mMediaSourceDuration;
};

} // namespace mozilla

#endif /* MOZILLA_MEDIASOURCEDECODER_H_ */
