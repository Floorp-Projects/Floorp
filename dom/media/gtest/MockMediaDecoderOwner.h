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
  nsresult DispatchAsyncEvent(const nsAString& aName) override
  {
    return NS_OK;
  }
  void FireTimeUpdate(bool aPeriodic) override {}
  bool GetPaused() override { return false; }
  void MetadataLoaded(const MediaInfo* aInfo,
                      nsAutoPtr<const MetadataTags> aTags) override
  {
  }
  void NetworkError() override {}
  void DecodeError() override {}
  bool HasError() const override { return false; }
  void LoadAborted() override {}
  void PlaybackEnded() override {}
  void SeekStarted() override {}
  void SeekCompleted() override {}
  void DownloadProgressed() override {}
  void UpdateReadyState() override {}
  void FirstFrameLoaded() override {}
#ifdef MOZ_EME
  void DispatchEncrypted(const nsTArray<uint8_t>& aInitData,
                         const nsAString& aInitDataType) override {}
#endif // MOZ_EME
  bool IsActive() const override { return true; }
  bool IsHidden() const override { return false; }
  void DownloadSuspended() override {}
  void DownloadResumed(bool aForceNetworkLoading) override {}
  void NotifySuspendedByCache(bool aIsSuspended) override {}
  void NotifyDecoderPrincipalChanged() override {}
  VideoFrameContainer* GetVideoFrameContainer() override
  {
    return nullptr;
  }
  void ResetConnectionState() override {}
  void SetAudibleState(bool aAudible) override {}
};
}

#endif
