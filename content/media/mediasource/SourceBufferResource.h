/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SOURCEBUFFERRESOURCE_H_
#define MOZILLA_SOURCEBUFFERRESOURCE_H_

#include "MediaCache.h"
#include "MediaResource.h"
#include "mozilla/Attributes.h"
#include "mozilla/ReentrantMonitor.h"
#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsIPrincipal.h"
#include "nsStringGlue.h"
#include "nsTArray.h"
#include "nscore.h"

class nsIStreamListener;

namespace mozilla {

class MediaDecoder;

namespace dom {

class SourceBuffer;

}  // namespace dom

class SourceBufferResource MOZ_FINAL : public MediaResource
{
public:
  SourceBufferResource(nsIPrincipal* aPrincipal,
                       const nsACString& aType);
  ~SourceBufferResource();

  nsresult Close();
  void Suspend(bool aCloseImmediately) {}
  void Resume() {}

  already_AddRefed<nsIPrincipal> GetCurrentPrincipal()
  {
    return nsCOMPtr<nsIPrincipal>(mPrincipal).forget();
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

  nsresult Read(char* aBuffer, uint32_t aCount, uint32_t* aBytes);
  nsresult ReadAt(int64_t aOffset, char* aBuffer, uint32_t aCount, uint32_t* aBytes);
  nsresult Seek(int32_t aWhence, int64_t aOffset);

  void StartSeekingForMetadata()
  {
  }

  void EndSeekingForMetadata()
  {
  }

  int64_t Tell()
  {
    return mOffset;
  }

  void Pin()
  {
  }

  void Unpin()
  {
  }

  double GetDownloadRate(bool* aIsReliable) { return 0; }
  int64_t GetLength() { return mInputBuffer.Length(); }
  int64_t GetNextCachedData(int64_t aOffset) { return aOffset; }
  int64_t GetCachedDataEnd(int64_t aOffset) { return GetLength(); }
  bool IsDataCachedToEndOfResource(int64_t aOffset) { return true; }
  bool IsSuspendedByCache() { return false; }
  bool IsSuspended() { return false; }
  nsresult ReadFromCache(char* aBuffer, int64_t aOffset, uint32_t aCount);

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

  bool IsTransportSeekable() MOZ_OVERRIDE { return true; }

  const nsCString& GetContentType() const MOZ_OVERRIDE
  {
    return mType;
  }

  // Used by SourceBuffer.
  void AppendData(const uint8_t* aData, uint32_t aLength);
  void Ended();

private:
  nsCOMPtr<nsIPrincipal> mPrincipal;
  const nsAutoCString mType;

  // Provides synchronization between SourceBuffers and InputAdapters.
  ReentrantMonitor mMonitor;
  nsTArray<uint8_t> mInputBuffer;

  int64_t mOffset;
  bool mClosed;
  bool mEnded;
};

} // namespace mozilla
#endif /* MOZILLA_SOURCEBUFFERRESOURCE_H_ */
