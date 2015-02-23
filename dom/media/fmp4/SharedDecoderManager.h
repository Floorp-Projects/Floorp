/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHARED_DECODER_MANAGER_H_
#define SHARED_DECODER_MANAGER_H_

#include "PlatformDecoderModule.h"
#include "mozilla/Monitor.h"

namespace mozilla
{

class MediaDataDecoder;
class SharedDecoderProxy;
class SharedDecoderCallback;

class SharedDecoderManager
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SharedDecoderManager)

  SharedDecoderManager();

  already_AddRefed<MediaDataDecoder> CreateVideoDecoder(
    PlatformDecoderModule* aPDM,
    const mp4_demuxer::VideoDecoderConfig& aConfig,
    layers::LayersBackend aLayersBackend,
    layers::ImageContainer* aImageContainer, FlushableMediaTaskQueue* aVideoTaskQueue,
    MediaDataDecoderCallback* aCallback);

  void SetReader(MediaDecoderReader* aReader);
  void Select(SharedDecoderProxy* aProxy);
  void SetIdle(MediaDataDecoder* aProxy);
  void ReleaseMediaResources();
  void Shutdown();

  friend class SharedDecoderProxy;
  friend class SharedDecoderCallback;

private:
  virtual ~SharedDecoderManager();
  void DrainComplete();

  nsRefPtr<MediaDataDecoder> mDecoder;
  nsRefPtr<FlushableMediaTaskQueue> mTaskQueue;
  SharedDecoderProxy* mActiveProxy;
  MediaDataDecoderCallback* mActiveCallback;
  nsAutoPtr<MediaDataDecoderCallback> mCallback;
  bool mWaitForInternalDrain;
  Monitor mMonitor;
  bool mDecoderReleasedResources;
};

class SharedDecoderProxy : public MediaDataDecoder
{
public:
  SharedDecoderProxy(SharedDecoderManager* aManager,
                     MediaDataDecoderCallback* aCallback);
  virtual ~SharedDecoderProxy();

  virtual nsresult Init() MOZ_OVERRIDE;
  virtual nsresult Input(mp4_demuxer::MP4Sample* aSample) MOZ_OVERRIDE;
  virtual nsresult Flush() MOZ_OVERRIDE;
  virtual nsresult Drain() MOZ_OVERRIDE;
  virtual nsresult Shutdown() MOZ_OVERRIDE;
  virtual bool IsWaitingMediaResources() MOZ_OVERRIDE;
  virtual bool IsDormantNeeded() MOZ_OVERRIDE;
  virtual void ReleaseMediaResources() MOZ_OVERRIDE;
  virtual void ReleaseDecoder() MOZ_OVERRIDE;
  virtual bool IsHardwareAccelerated() const MOZ_OVERRIDE;

  friend class SharedDecoderManager;

private:
  nsRefPtr<SharedDecoderManager> mManager;
  MediaDataDecoderCallback* mCallback;
};
}

#endif
