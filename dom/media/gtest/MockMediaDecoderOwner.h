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
  virtual void DownloadStalled() MOZ_OVERRIDE {}
  virtual nsresult DispatchEvent(const nsAString& aName) MOZ_OVERRIDE
  {
    return NS_OK;
  }
  virtual nsresult DispatchAsyncEvent(const nsAString& aName) MOZ_OVERRIDE
  {
    return NS_OK;
  }
  virtual void FireTimeUpdate(bool aPeriodic) MOZ_OVERRIDE {}
  virtual bool GetPaused() MOZ_OVERRIDE { return false; }
  virtual void MetadataLoaded(const MediaInfo* aInfo, const MetadataTags* aTags)
    MOZ_OVERRIDE
  {
  }
  virtual void NetworkError() MOZ_OVERRIDE {}
  virtual void DecodeError() MOZ_OVERRIDE {}
  virtual void LoadAborted() MOZ_OVERRIDE {}
  virtual void PlaybackEnded() MOZ_OVERRIDE {}
  virtual void SeekStarted() MOZ_OVERRIDE {}
  virtual void SeekCompleted() MOZ_OVERRIDE {}
  virtual void DownloadSuspended() MOZ_OVERRIDE {}
  virtual void DownloadResumed(bool aForceNetworkLoading) MOZ_OVERRIDE {}
  virtual void NotifySuspendedByCache(bool aIsSuspended) MOZ_OVERRIDE {}
  virtual void NotifyDecoderPrincipalChanged() MOZ_OVERRIDE {}
  virtual void UpdateReadyStateForData(NextFrameStatus aNextFrame) MOZ_OVERRIDE
  {
  }
  virtual VideoFrameContainer* GetVideoFrameContainer() MOZ_OVERRIDE
  {
    return nullptr;
  }
  virtual void ResetConnectionState() MOZ_OVERRIDE {}
};
}

#endif
