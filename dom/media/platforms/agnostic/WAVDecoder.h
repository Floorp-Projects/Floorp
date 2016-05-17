/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(WaveDecoder_h_)
#define WaveDecoder_h_

#include "PlatformDecoderModule.h"
#include "mp4_demuxer/ByteReader.h"

namespace mozilla {

class WaveDataDecoder : public MediaDataDecoder
{
public:
  WaveDataDecoder(const AudioInfo& aConfig,
                  TaskQueue* aTaskQueue,
                  MediaDataDecoderCallback* aCallback);

  // Return true if mimetype is Wave
  static bool IsWave(const nsACString& aMimeType);

private:
  RefPtr<InitPromise> Init() override;
  nsresult Input(MediaRawData* aSample) override;
  nsresult Flush() override;
  nsresult Drain() override;
  nsresult Shutdown() override;
  const char* GetDescriptionName() const override
  {
    return "wave audio decoder";
  }

  void ProcessDecode(MediaRawData* aSample);
  bool DoDecode(MediaRawData* aSample);
  void ProcessDrain();

  const AudioInfo& mInfo;
  const RefPtr<TaskQueue> mTaskQueue;
  MediaDataDecoderCallback* mCallback;
  Atomic<bool> mIsFlushing;

  int64_t mFrames;
};

} // namespace mozilla
#endif
