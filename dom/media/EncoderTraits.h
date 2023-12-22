/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EncoderTraits_h_
#define EncoderTraits_h_

#include "mozilla/dom/EncoderTypes.h"
#include "PEMFactory.h"

namespace mozilla::EncoderSupport {

bool Supports(const RefPtr<dom::VideoEncoderConfigInternal>& aEncoderConfigInternal) {
  RefPtr<PEMFactory> factory = new PEMFactory();
  EncoderConfig config = aEncoderConfigInternal->ToEncoderConfig();
  return factory->Supports(config);
}

}  // namespace mozilla

#endif
