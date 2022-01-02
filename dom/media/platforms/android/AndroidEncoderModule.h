/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORMS_ANDROID_ANDROIDENCODERMODULE_H_
#define DOM_MEDIA_PLATFORMS_ANDROID_ANDROIDENCODERMODULE_H_

#include "PlatformEncoderModule.h"

namespace mozilla {

class AndroidEncoderModule final : public PlatformEncoderModule {
 public:
  bool SupportsMimeType(const nsACString& aMimeType) const override;

  already_AddRefed<MediaDataEncoder> CreateVideoEncoder(
      const CreateEncoderParams& aParams) const override;
};

}  // namespace mozilla

#endif
