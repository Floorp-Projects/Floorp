/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AppleEncoderModule_h_
#define AppleEncoderModule_h_

#include "PlatformEncoderModule.h"

namespace mozilla {
class AppleEncoderModule final : public PlatformEncoderModule {
 public:
  virtual ~AppleEncoderModule() = default;

  bool Supports(const EncoderConfig& aConfig) const override;
  bool SupportsCodec(CodecType aCodec) const override;

  const char* GetName() const override { return "Apple Encoder Module"; }

  already_AddRefed<MediaDataEncoder> CreateVideoEncoder(
      const EncoderConfig& aConfig,
      const RefPtr<TaskQueue>& aTaskQueue) const override;
};

}  // namespace mozilla

#endif /* AppleEncoderModule_h_ */
