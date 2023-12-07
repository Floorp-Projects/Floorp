/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(PEMFactory_h_)
#  define PEMFactory_h_

#  include "PlatformEncoderModule.h"

namespace mozilla {

using PEMCreateEncoderPromise = PlatformEncoderModule::CreateEncoderPromise;

class PEMFactory final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PEMFactory)

  PEMFactory();

  // Factory method that creates the appropriate PlatformEncoderModule for
  // the platform we're running on. Caller is responsible for deleting this
  // instance. It's expected that there will be multiple
  // PlatformEncoderModules alive at the same time.
  already_AddRefed<MediaDataEncoder> CreateEncoder(
      const EncoderConfig& aConfig, const RefPtr<TaskQueue>& aTaskQueue);

  RefPtr<PlatformEncoderModule::CreateEncoderPromise> CreateEncoderAsync(
      const EncoderConfig& aConfig, const RefPtr<TaskQueue>& aTaskQueue);

  bool Supports(const EncoderConfig& aConfig) const;
  bool SupportsCodec(CodecType aCodec) const;

 private:
  RefPtr<PlatformEncoderModule::CreateEncoderPromise>
  CheckAndMaybeCreateEncoder(const EncoderConfig& aConfig, uint32_t aIndex,
                             const RefPtr<TaskQueue>& aTaskQueue);

  RefPtr<PlatformEncoderModule::CreateEncoderPromise> CreateEncoderWithPEM(
      PlatformEncoderModule* aPEM, const EncoderConfig& aConfig,
      const RefPtr<TaskQueue>& aTaskQueue);
  virtual ~PEMFactory() = default;
  // Returns the first PEM in our list supporting the codec.
  already_AddRefed<PlatformEncoderModule> FindPEM(
      const EncoderConfig& aConfig) const;

  nsTArray<RefPtr<PlatformEncoderModule>> mCurrentPEMs;
};

}  // namespace mozilla

#endif /* PEMFactory_h_ */
