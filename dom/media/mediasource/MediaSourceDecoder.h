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
#include "MediaFormatReader.h"

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
  explicit MediaSourceDecoder(MediaDecoderInit& aInit);

  MediaDecoderStateMachine* CreateStateMachine() override;
  nsresult Load(nsIStreamListener**) override;
  media::TimeIntervals GetSeekable() override;
  media::TimeIntervals GetBuffered() override;

  void Shutdown() override;

  void AttachMediaSource(dom::MediaSource* aMediaSource);
  void DetachMediaSource();

  void Ended(bool aEnded);

  // Return the duration of the video in seconds.
  double GetDuration() override;

  void SetInitialDuration(int64_t aDuration);
  void SetMediaSourceDuration(double aDuration);

  MediaSourceDemuxer* GetDemuxer()
  {
    return mDemuxer;
  }

  // Returns a string describing the state of the MediaSource internal
  // buffered data. Used for debugging purposes.
  void GetMozDebugReaderData(nsACString& aString) override;

  void AddSizeOfResources(ResourceSizes* aSizes) override;

  MediaDecoderOwner::NextFrameStatus NextFrameBufferedStatus() override;
  bool CanPlayThrough() override;

  void NotifyWaitingForKey() override;

  MediaEventSource<void>* WaitingForKeyEvent() override;

  bool IsMSE() const override { return true; }

  void NotifyInitDataArrived();

  void CreateResource(nsIPrincipal* aPrincipal);

private:
  void DoSetMediaSourceDuration(double aDuration);
  media::TimeInterval ClampIntervalToEnd(const media::TimeInterval& aInterval);

  // The owning MediaSource holds a strong reference to this decoder, and
  // calls Attach/DetachMediaSource on this decoder to set and clear
  // mMediaSource.
  dom::MediaSource* mMediaSource;
  RefPtr<MediaSourceDemuxer> mDemuxer;
  RefPtr<MediaFormatReader> mReader;
  MediaEventProducer<void> mWaitingForKeyEvent;

  bool mEnded;
};

} // namespace mozilla

#endif /* MOZILLA_MEDIASOURCEDECODER_H_ */
