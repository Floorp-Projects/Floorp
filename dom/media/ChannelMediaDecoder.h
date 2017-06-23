/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ChannelMediaDecoder_h_
#define ChannelMediaDecoder_h_

#include "MediaDecoder.h"
#include "MediaResourceCallback.h"

class nsIChannel;
class nsIStreamListener;

namespace mozilla {

class ChannelMediaDecoder : public MediaDecoder
{
  // Used to register with MediaResource to receive notifications which will
  // be forwarded to MediaDecoder.
  class ResourceCallback : public MediaResourceCallback
  {
    // Throttle calls to MediaDecoder::NotifyDataArrived()
    // to be at most once per 500ms.
    static const uint32_t sDelay = 500;

  public:
    explicit ResourceCallback(AbstractThread* aMainThread);
    // Start to receive notifications from ResourceCallback.
    void Connect(ChannelMediaDecoder* aDecoder);
    // Called upon shutdown to stop receiving notifications.
    void Disconnect();

  private:
    /* MediaResourceCallback functions */
    MediaDecoderOwner* GetMediaOwner() const override;
    void SetInfinite(bool aInfinite) override;
    void NotifyNetworkError() override;
    void NotifyDataArrived() override;
    void NotifyDataEnded(nsresult aStatus) override;
    void NotifyPrincipalChanged() override;
    void NotifySuspendedStatusChanged() override;
    void NotifyBytesConsumed(int64_t aBytes, int64_t aOffset) override;

    static void TimerCallback(nsITimer* aTimer, void* aClosure);

    // The decoder to send notifications. Main-thread only.
    ChannelMediaDecoder* mDecoder = nullptr;
    nsCOMPtr<nsITimer> mTimer;
    bool mTimerArmed = false;
    const RefPtr<AbstractThread> mAbstractMainThread;
  };

  RefPtr<ResourceCallback> mResourceCallback;

public:
  explicit ChannelMediaDecoder(MediaDecoderInit& aInit);

  void Shutdown() override;

  // Create a new decoder of the same type as this one.
  // Subclasses must implement this.
  virtual ChannelMediaDecoder* Clone(MediaDecoderInit& aInit) = 0;

  nsresult CreateResource(nsIChannel* aChannel, bool aIsPrivateBrowsing);
  nsresult CreateResource(MediaResource* aOriginal);
  nsresult Load(nsIStreamListener** aStreamListener);

private:
  nsresult OpenResource(nsIStreamListener** aStreamListener);
};

} // namespace mozilla

#endif // ChannelMediaDecoder_h_
