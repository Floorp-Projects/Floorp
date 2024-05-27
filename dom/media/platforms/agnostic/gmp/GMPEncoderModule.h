/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPEncoderModule_h_
#define GMPEncoderModule_h_

#include "PlatformEncoderModule.h"

namespace mozilla {
class GMPEncoderModule final : public PlatformEncoderModule {
 public:
  GMPEncoderModule() = default;

  already_AddRefed<MediaDataEncoder> CreateVideoEncoder(
      const EncoderConfig& aConfig,
      const RefPtr<TaskQueue>& aTaskQueue) const override;

  bool Supports(const EncoderConfig& aConfig) const override;
  bool SupportsCodec(CodecType aCodecType) const override;

  const char* GetName() const override { return "GMP Encoder Module"; }
};

}  // namespace mozilla

#endif /* GMPEncoderModule_h_ */
