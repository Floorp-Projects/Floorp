/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WMFEncoderModule_h_
#define WMFEncoderModule_h_

#include "PlatformEncoderModule.h"

namespace mozilla {
class WMFEncoderModule final : public PlatformEncoderModule {
 public:
  bool SupportsMimeType(const nsACString& aMimeType) const override;

  already_AddRefed<MediaDataEncoder> CreateVideoEncoder(
      const CreateEncoderParams& aParams) const override;
};

}  // namespace mozilla

#endif /* WMFEncoderModule_h_ */
