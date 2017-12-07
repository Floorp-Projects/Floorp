/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_MEDIASOURCEDECODER_H_
#define MOZILLA_MEDIASOURCEDECODER_H_

#include "MediaDecoder.h"
#include "mozilla/RefPtr.h"

namespace mozilla {

class MediaDecoderStateMachine;
class MediaSourceDemuxer;

namespace dom {

class MediaSource;

} // namespace dom

DDLoggedTypeDeclNameAndBase(MediaSourceDecoder, MediaDecoder);

class MediaSourceDecoder
  : public MediaDecoder
  , public DecoderDoctorLifeLogger<MediaSourceDecoder>
{
public:
  explicit MediaSourceDecoder(MediaDecoderInit& aInit);

  nsresult Load(nsIPrincipal* aPrincipal);
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

  already_AddRefed<nsIPrincipal> GetCurrentPrincipal() override;

  bool IsTransportSeekable() override { return true; }

  // Returns a string describing the state of the MediaSource internal
  // buffered data. Used for debugging purposes.
  void GetMozDebugReaderData(nsACString& aString) override;

  void AddSizeOfResources(ResourceSizes* aSizes) override;

  MediaDecoderOwner::NextFrameStatus NextFrameBufferedStatus() override;

  bool IsMSE() const override { return true; }

  void NotifyInitDataArrived();

  // Called as data appended to the source buffer or EOS is called on the media
  // source. Main thread only.
  void NotifyDataArrived();

private:
  void PinForSeek() override {}
  void UnpinForSeek() override {}
  MediaDecoderStateMachine* CreateStateMachine();
  void DoSetMediaSourceDuration(double aDuration);
  media::TimeInterval ClampIntervalToEnd(const media::TimeInterval& aInterval);
  bool CanPlayThroughImpl() override;
  bool IsLiveStream() override final { return !mEnded; }

  RefPtr<nsIPrincipal> mPrincipal;

  // The owning MediaSource holds a strong reference to this decoder, and
  // calls Attach/DetachMediaSource on this decoder to set and clear
  // mMediaSource.
  dom::MediaSource* mMediaSource;
  RefPtr<MediaSourceDemuxer> mDemuxer;

  bool mEnded;
};

} // namespace mozilla

#endif /* MOZILLA_MEDIASOURCEDECODER_H_ */
