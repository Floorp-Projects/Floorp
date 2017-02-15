/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SamplesWaitingForKey_h_
#define SamplesWaitingForKey_h_

#include "MediaInfo.h"
#include "mozilla/MozPromise.h"
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"

namespace mozilla {

typedef nsTArray<uint8_t> CencKeyId;

class CDMProxy;
template <typename... Es> class MediaEventProducer;
class MediaRawData;

// Encapsulates the task of waiting for the CDMProxy to have the necessary
// keys to decrypt a given sample.
class SamplesWaitingForKey
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SamplesWaitingForKey)

  typedef MozPromise<RefPtr<MediaRawData>, bool, /* IsExclusive = */ true>
    WaitForKeyPromise;

  SamplesWaitingForKey(
    CDMProxy* aProxy,
    TrackInfo::TrackType aType,
    MediaEventProducer<TrackInfo::TrackType>* aOnWaitingForKey);

  // Returns a promise that will be resolved if or when a key for decoding the
  // sample becomes usable.
  RefPtr<WaitForKeyPromise> WaitIfKeyNotUsable(MediaRawData* aSample);

  void NotifyUsable(const CencKeyId& aKeyId);

  void Flush();

protected:
  ~SamplesWaitingForKey();

private:
  Mutex mMutex;
  RefPtr<CDMProxy> mProxy;
  struct SampleEntry
  {
    RefPtr<MediaRawData> mSample;
    MozPromiseHolder<WaitForKeyPromise> mPromise;
  };
  nsTArray<SampleEntry> mSamples;
  const TrackInfo::TrackType mType;
  MediaEventProducer<TrackInfo::TrackType>* const mOnWaitingForKeyEvent;
};

} // namespace mozilla

#endif //  SamplesWaitingForKey_h_
