/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOCK_MEDIA_DECODER_OWNER_H_
#define MOCK_MEDIA_DECODER_OWNER_H_

#include "MediaDecoderOwner.h"

namespace mozilla
{

class MockMediaDecoderOwner : public MediaDecoderOwner
{
public:
  virtual nsresult DispatchAsyncEvent(const nsAString& aName) override
  {
    return NS_OK;
  }
  virtual void FireTimeUpdate(bool aPeriodic) override {}
  virtual bool GetPaused() override { return false; }
  virtual void MetadataLoaded(const MediaInfo* aInfo,
                              nsAutoPtr<const MetadataTags> aTags) override
  {
  }
  virtual void NetworkError() override {}
  virtual void DecodeError() override {}
  virtual void LoadAborted() override {}
  virtual void PlaybackEnded() override {}
  virtual void SeekStarted() override {}
  virtual void SeekCompleted() override {}
  virtual void DownloadProgressed() override {}
  virtual void UpdateReadyState() override {}
  virtual void FirstFrameLoaded() override {}
#ifdef MOZ_EME
  virtual void DispatchEncrypted(const nsTArray<uint8_t>& aInitData,
                                 const nsAString& aInitDataType) override {}
#endif // MOZ_EME
  virtual bool IsActive() const override { return true; }
  virtual bool IsHidden() const override { return false; }
  virtual void DownloadSuspended() override {}
  virtual void DownloadResumed(bool aForceNetworkLoading) override {}
  virtual void NotifySuspendedByCache(bool aIsSuspended) override {}
  virtual void NotifyDecoderPrincipalChanged() override {}
  virtual VideoFrameContainer* GetVideoFrameContainer() override
  {
    return nullptr;
  }
  virtual void ResetConnectionState() override {}
};
}

#endif
