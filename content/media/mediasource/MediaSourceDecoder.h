/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_MEDIASOURCEDECODER_H_
#define MOZILLA_MEDIASOURCEDECODER_H_

#include "MediaCache.h"
#include "MediaDecoder.h"
#include "MediaResource.h"
#include "mozilla/Attributes.h"
#include "mozilla/ReentrantMonitor.h"
#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsStringGlue.h"
#include "nsTArray.h"

class nsIPrincipal;
class nsIStreamListener;

namespace mozilla {

class MediaDecoderReader;
class MediaDecoderStateMachine;
class SubBufferDecoder;

namespace dom {

class HTMLMediaElement;
class MediaSource;

} // namespace dom

class MediaSourceDecoder : public MediaDecoder
{
public:
  MediaSourceDecoder(HTMLMediaElement* aElement);

  MediaDecoder* Clone() MOZ_OVERRIDE;
  MediaDecoderStateMachine* CreateStateMachine() MOZ_OVERRIDE;
  nsresult Load(nsIStreamListener**, MediaDecoder*) MOZ_OVERRIDE;

  void AttachMediaSource(MediaSource* aMediaSource);
  void DetachMediaSource();

  SubBufferDecoder* CreateSubDecoder(const nsACString& aType);

  const nsTArray<MediaDecoderReader*>& GetReaders()
  {
    ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
    while (mReaders.Length() == 0) {
      mon.Wait();
    }
    return mReaders;
  }

  void SetVideoReader(MediaDecoderReader* aReader)
  {
    MOZ_ASSERT(aReader && !mVideoReader);
    mVideoReader = aReader;
  }

  void SetAudioReader(MediaDecoderReader* aReader)
  {
    MOZ_ASSERT(aReader && !mAudioReader);
    mAudioReader = aReader;
  }

  MediaDecoderReader* GetVideoReader()
  {
    return mVideoReader;
  }

  MediaDecoderReader* GetAudioReader()
  {
    return mAudioReader;
  }

private:
  MediaSource* mMediaSource;

  nsTArray<nsRefPtr<SubBufferDecoder> > mDecoders;
  nsTArray<MediaDecoderReader*> mReaders; // Readers owned by Decoders.

  MediaDecoderReader* mVideoReader;
  MediaDecoderReader* mAudioReader;
};

class MediaSourceResource MOZ_FINAL : public MediaResource
{
public:
  MediaSourceResource()
  {
  }

  nsresult Close()
  {
    return NS_OK;
  }

  void Suspend(bool aCloseImmediately)
  {
  }

  void Resume()
  {
  }

  already_AddRefed<nsIPrincipal> GetCurrentPrincipal()
  {
    return nullptr;
  }

  bool CanClone()
  {
    return false;
  }

  already_AddRefed<MediaResource> CloneData(MediaDecoder* aDecoder)
  {
    return nullptr;
  }

  void SetReadMode(MediaCacheStream::ReadMode aMode)
  {
  }

  void SetPlaybackRate(uint32_t aBytesPerSecond)
  {
  }

  nsresult Read(char* aBuffer, uint32_t aCount, uint32_t* aBytes)
  {
    return NS_ERROR_FAILURE;
  }

  nsresult ReadAt(int64_t aOffset, char* aBuffer, uint32_t aCount, uint32_t* aBytes)
  {
    return NS_ERROR_FAILURE;
  }

  nsresult Seek(int32_t aWhence, int64_t aOffset)
  {
    return NS_ERROR_FAILURE;
  }

  void StartSeekingForMetadata()
  {
  }

  void EndSeekingForMetadata()
  {
  }

  int64_t Tell()
  {
    return -1;
  }

  void Pin()
  {
  }

  void Unpin()
  {
  }

  double GetDownloadRate(bool* aIsReliable)
  {
    return 0;
  }

  int64_t GetLength()
  {
    return -1;
  }

  int64_t GetNextCachedData(int64_t aOffset)
  {
    return aOffset;
  }

  int64_t GetCachedDataEnd(int64_t aOffset)
  {
    return GetLength();
  }

  bool IsDataCachedToEndOfResource(int64_t aOffset)
  {
    return true;
  }

  bool IsSuspendedByCache()
  {
    return false;
  }

  bool IsSuspended()
  {
    return false;
  }

  nsresult ReadFromCache(char* aBuffer, int64_t aOffset, uint32_t aCount)
  {
    return NS_ERROR_FAILURE;
  }


  nsresult Open(nsIStreamListener** aStreamListener)
  {
    return NS_ERROR_FAILURE;
  }

#ifdef MOZ_DASH
  nsresult OpenByteRange(nsIStreamListener** aStreamListener,
                         const MediaByteRange& aByteRange)
  {
    return NS_ERROR_FAILURE;
  }
#endif

  nsresult GetCachedRanges(nsTArray<MediaByteRange>& aRanges)
  {
    aRanges.AppendElement(MediaByteRange(0, GetLength()));
    return NS_OK;
  }

  bool IsTransportSeekable() MOZ_OVERRIDE
  {
    return true;
  }

  const nsCString& GetContentType() const MOZ_OVERRIDE
  {
    return mType;
  }

private:
  const nsAutoCString mType;
};

} // namespace mozilla

#endif /* MOZILLA_MEDIASOURCEDECODER_H_ */
