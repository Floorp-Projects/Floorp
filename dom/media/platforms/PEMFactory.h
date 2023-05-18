/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(PEMFactory_h_)
#  define PEMFactory_h_

#  include "PlatformEncoderModule.h"

namespace mozilla {

class PEMFactory final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PEMFactory)

  PEMFactory();

  // Factory method that creates the appropriate PlatformEncoderModule for
  // the platform we're running on. Caller is responsible for deleting this
  // instance. It's expected that there will be multiple
  // PlatformEncoderModules alive at the same time.
  already_AddRefed<MediaDataEncoder> CreateEncoder(
      const CreateEncoderParams& aParams, const bool aHardwareNotAllowed);

  bool SupportsMimeType(const nsACString& aMimeType) const;

 private:
  virtual ~PEMFactory() = default;
  // Returns the first PEM in our list supporting the mimetype.
  already_AddRefed<PlatformEncoderModule> FindPEM(
      const TrackInfo& aTrackInfo) const;

  nsTArray<RefPtr<PlatformEncoderModule>> mModules;
};

}  // namespace mozilla

#endif /* PEMFactory_h_ */
