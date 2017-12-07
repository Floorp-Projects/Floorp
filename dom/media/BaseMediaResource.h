/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BaseMediaResource_h
#define BaseMediaResource_h

#include "MediaResource.h"
#include "MediaResourceCallback.h"
#include "MediaCache.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsIStreamListener.h"

class nsIPrincipal;

namespace mozilla {

DDLoggedTypeDeclNameAndBase(BaseMediaResource, MediaResource);

class BaseMediaResource
  : public MediaResource
  , public DecoderDoctorLifeLogger<BaseMediaResource>
{
public:
  /**
   * Create a resource, reading data from the channel. Call on main thread only.
   * The caller must follow up by calling resource->Open().
   */
  static already_AddRefed<BaseMediaResource> Create(
    MediaResourceCallback* aCallback,
    nsIChannel* aChannel,
    bool aIsPrivateBrowsing);

  // Pass true to limit the amount of readahead data (specified by
  // "media.cache_readahead_limit") or false to read as much as the
  // cache size allows.
  virtual void ThrottleReadahead(bool bThrottle) {}

  // This is the client's estimate of the playback rate assuming
  // the media plays continuously. The cache can't guess this itself
  // because it doesn't know when the decoder was paused, buffering, etc.
  virtual void SetPlaybackRate(uint32_t aBytesPerSecond) = 0;

  // Get the estimated download rate in bytes per second (assuming no
  // pausing of the channel is requested by Gecko).
  // *aIsReliable is set to true if we think the estimate is useful.
  virtual double GetDownloadRate(bool* aIsReliable) = 0;

  // Moves any existing channel loads into or out of background. Background
  // loads don't block the load event. This also determines whether or not any
  // new loads initiated (for example to seek) will be in the background.
  void SetLoadInBackground(bool aLoadInBackground);

  // Suspend any downloads that are in progress.
  // If aCloseImmediately is set, resources should be released immediately
  // since we don't expect to resume again any time soon. Otherwise we
  // may resume again soon so resources should be held for a little
  // while.
  virtual void Suspend(bool aCloseImmediately) = 0;

  // Resume any downloads that have been suspended.
  virtual void Resume() = 0;

  // The mode is initially MODE_METADATA.
  virtual void SetReadMode(MediaCacheStream::ReadMode aMode) = 0;

  // Returns true if the resource can be seeked to unbuffered ranges, i.e.
  // for an HTTP network stream this returns true if HTTP1.1 Byte Range
  // requests are supported by the connection/server.
  virtual bool IsTransportSeekable() = 0;

  // Get the current principal for the channel
  virtual already_AddRefed<nsIPrincipal> GetCurrentPrincipal() = 0;

  /**
   * Open the stream. This creates a stream listener and returns it in
   * aStreamListener; this listener needs to be notified of incoming data.
   */
  virtual nsresult Open(nsIStreamListener** aStreamListener) = 0;

  // If this returns false, then we shouldn't try to clone this MediaResource
  // because its underlying resources are not suitable for reuse (e.g.
  // because the underlying connection has been lost, or this resource
  // just can't be safely cloned). If this returns true, CloneData could
  // still fail. If this returns false, CloneData should not be called.
  virtual bool CanClone() { return false; }

  // Create a new stream of the same type that refers to the same URI
  // with a new channel. Any cached data associated with the original
  // stream should be accessible in the new stream too.
  virtual already_AddRefed<BaseMediaResource> CloneData(
    MediaResourceCallback* aCallback)
  {
    return nullptr;
  }

  // Returns true if the resource is a live stream.
  bool IsLiveStream() { return GetLength() == -1; }

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
  {
    // Might be useful to track in the future:
    // - mChannel
    // - mURI (possibly owned, looks like just a ref from mChannel)
    // Not owned:
    // - mCallback
    return 0;
  }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  virtual nsCString GetDebugInfo() { return nsCString(); }

protected:
  BaseMediaResource(MediaResourceCallback* aCallback,
                    nsIChannel* aChannel,
                    nsIURI* aURI)
    : mCallback(aCallback)
    , mChannel(aChannel)
    , mURI(aURI)
    , mLoadInBackground(false)
  {
  }
  virtual ~BaseMediaResource() {}

  // Set the request's load flags to aFlags.  If the request is part of a
  // load group, the request is removed from the group, the flags are set, and
  // then the request is added back to the load group.
  void ModifyLoadFlags(nsLoadFlags aFlags);

  // Dispatches an event to call MediaDecoder::NotifyBytesConsumed(aNumBytes, aOffset)
  // on the main thread. This is called automatically after every read.
  void DispatchBytesConsumed(int64_t aNumBytes, int64_t aOffset);

  RefPtr<MediaResourceCallback> mCallback;

  // Channel used to download the media data. Must be accessed
  // from the main thread only.
  nsCOMPtr<nsIChannel> mChannel;

  // URI in case the stream needs to be re-opened. Access from
  // main thread only.
  nsCOMPtr<nsIURI> mURI;

  // True if SetLoadInBackground() has been called with
  // aLoadInBackground = true, i.e. when the document load event is not
  // blocked by this resource, and all channel loads will be in the
  // background.
  bool mLoadInBackground;
};

} // namespace mozilla

#endif // BaseMediaResource_h
