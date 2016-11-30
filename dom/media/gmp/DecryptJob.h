/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DecryptJob_h_
#define DecryptJob_h_

#include "mozilla/CDMProxy.h"

namespace mozilla {

class DecryptJob {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DecryptJob)

  explicit DecryptJob(MediaRawData* aSample)
    : mId(0)
    , mSample(aSample)
  {
  }

  void PostResult(DecryptStatus aResult,
                  const nsTArray<uint8_t>& aDecryptedData);
  void PostResult(DecryptStatus aResult);

  RefPtr<DecryptPromise> Ensure() {
    return mPromise.Ensure(__func__);
  }

  uint32_t mId;
  RefPtr<MediaRawData> mSample;
private:
  ~DecryptJob() {}
  MozPromiseHolder<DecryptPromise> mPromise;
};

} // namespace mozilla

#endif // DecryptJob_h_
