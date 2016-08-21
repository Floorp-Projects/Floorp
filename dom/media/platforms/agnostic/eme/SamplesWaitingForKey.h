/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SamplesWaitingForKey_h_
#define SamplesWaitingForKey_h_

#include "mozilla/TaskQueue.h"

#include "PlatformDecoderModule.h"

namespace mozilla {

typedef nsTArray<uint8_t> CencKeyId;

class CDMProxy;

// Encapsulates the task of waiting for the CDMProxy to have the necessary
// keys to decrypt a given sample.
class SamplesWaitingForKey {
public:

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SamplesWaitingForKey)

  explicit SamplesWaitingForKey(MediaDataDecoder* aDecoder,
                                MediaDataDecoderCallback* aCallback,
                                TaskQueue* aTaskQueue,
                                CDMProxy* aProxy);

  // Returns true if we need to wait for a key to become usable.
  // Will callback MediaDataDecoder::Input(aSample) on mDecoder once the
  // sample is ready to be decrypted. The order of input samples is
  // preserved.
  bool WaitIfKeyNotUsable(MediaRawData* aSample);

  void NotifyUsable(const CencKeyId& aKeyId);

  void Flush();

  void BreakCycles();

protected:
  ~SamplesWaitingForKey();

private:
  Mutex mMutex;
  RefPtr<MediaDataDecoder> mDecoder;
  MediaDataDecoderCallback* mDecoderCallback;
  RefPtr<TaskQueue> mTaskQueue;
  RefPtr<CDMProxy> mProxy;
  nsTArray<RefPtr<MediaRawData>> mSamples;
};

} // namespace mozilla

#endif //  SamplesWaitingForKey_h_
