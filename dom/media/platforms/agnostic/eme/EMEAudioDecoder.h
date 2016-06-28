/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EMEAudioDecoder_h_
#define EMEAudioDecoder_h_

#include "GMPAudioDecoder.h"
#include "PlatformDecoderModule.h"

namespace mozilla {

class EMEAudioCallbackAdapter : public AudioCallbackAdapter {
public:
  explicit EMEAudioCallbackAdapter(MediaDataDecoderCallbackProxy* aCallback)
   : AudioCallbackAdapter(aCallback)
  {}

  void Error(GMPErr aErr) override;
};

class EMEAudioDecoder : public GMPAudioDecoder {
public:
  EMEAudioDecoder(CDMProxy* aProxy, const GMPAudioDecoderParams& aParams);

private:
  void InitTags(nsTArray<nsCString>& aTags) override;
  nsCString GetNodeId() override;

  RefPtr<CDMProxy> mProxy;
};

} // namespace mozilla

#endif
