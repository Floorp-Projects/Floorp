/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaResourceCallback_h_
#define MediaResourceCallback_h_

#include "nsError.h"
#include "nsISupportsImpl.h"
#include "MediaResult.h"

namespace mozilla {

class AbstractThread;
class MediaDecoderOwner;
class MediaResource;

/**
 * A callback used by MediaResource (sub-classes like FileMediaResource,
 * RtspMediaResource, and ChannelMediaResource) to notify various events.
 * Currently this is implemented by MediaDecoder only.
 *
 * Since this class has no pure virtual function, it is convenient to write
 * gtests for the readers without using a mock MediaResource when you don't
 * care about the events notified by the MediaResource.
 */
class MediaResourceCallback {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaResourceCallback);

  // Return an abstract thread on which to run main thread runnables.
  virtual AbstractThread* AbstractMainThread() const { return nullptr; }

  // Returns a weak reference to the media decoder owner.
  virtual MediaDecoderOwner* GetMediaOwner() const { return nullptr; }

  // Notify that a network error is encountered.
  virtual void NotifyNetworkError(const MediaResult& aError) {}

  // Notify that data arrives on the stream and is read into the cache.
  virtual void NotifyDataArrived() {}

  // Notify download is ended.
  // NOTE: this can be called with the media cache lock held, so don't
  // block or do anything which might try to acquire a lock!
  virtual void NotifyDataEnded(nsresult aStatus) {}

  // Notify that the principal of MediaResource has changed.
  virtual void NotifyPrincipalChanged() {}

  // Notify that the "cache suspended" status of MediaResource changes.
  virtual void NotifySuspendedStatusChanged(bool aSuspendedByCache) {}

protected:
  virtual ~MediaResourceCallback() {}
};

} // namespace mozilla

#endif //MediaResourceCallback_h_
