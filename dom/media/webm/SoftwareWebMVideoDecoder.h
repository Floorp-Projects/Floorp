/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(SoftwareWebMVideoDecoder_h_)
#define SoftwareWebMVideoDecoder_h_

#include <stdint.h>

#include "WebMReader.h"

namespace mozilla {

class SoftwareWebMVideoDecoder : public WebMVideoDecoder
{
public:
  static WebMVideoDecoder* Create(WebMReader* aReader);

  virtual nsresult Init(unsigned int aWidth = 0,
                        unsigned int aHeight = 0) override;

  virtual bool DecodeVideoFrame(bool &aKeyframeSkip,
                                int64_t aTimeThreshold) override;

  virtual void Shutdown() override;

  explicit SoftwareWebMVideoDecoder(WebMReader* aReader);
  ~SoftwareWebMVideoDecoder();

private:
  nsresult InitDecoder(unsigned int aWidth, unsigned int aHeight);
  nsRefPtr<WebMReader> mReader;

  // VPx decoder state
  vpx_codec_ctx_t mVPX;
};

} // namespace mozilla

#endif
