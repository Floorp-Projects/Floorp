/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidEncoderModule.h"

#include "AndroidDataEncoder.h"
#include "MP4Decoder.h"

#include "mozilla/Logging.h"

namespace mozilla {
extern LazyLogModule sPEMLog;
#define AND_PEM_LOG(arg, ...)            \
  MOZ_LOG(                               \
      sPEMLog, mozilla::LogLevel::Debug, \
      ("AndroidEncoderModule(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

bool AndroidEncoderModule::SupportsMimeType(const nsACString& aMimeType) const {
  return MP4Decoder::IsH264(aMimeType);
}

already_AddRefed<MediaDataEncoder> AndroidEncoderModule::CreateVideoEncoder(
    const CreateEncoderParams& aParams) const {
  RefPtr<MediaDataEncoder> encoder =
      new AndroidDataEncoder(aParams.ToH264Config(), aParams.mTaskQueue);
  return encoder.forget();
}

}  // namespace mozilla

#undef AND_PEM_LOG
