/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _SDPTELEMETRY_H_
#define _SDPTELEMETRY_H_

#include "sdp/SdpParser.h"
#include "sdp/SdpPref.h"

namespace mozilla {

class SdpTelemetry {
 public:
  SdpTelemetry() = delete;

  using Results = UniquePtr<SdpParser::Results>;
  using Modes = SdpPref::AlternateParseModes;

  enum class Roles {
    Primary,
    Secondary,
  };

  static auto RecordParse(const Results& aResults, const Modes& aMode,
                          const Roles& aRole) -> void;

  static auto RecordSecondaryParse(const Results& aResult, const Modes& aMode)
      -> void;

  static auto RecordCompare(const Results& aFirst, const Results& aSecond,
                            const Modes& aMode) -> void;

 private:
  static auto BucketNameFragment(const Results& aResult, const Modes& aModes,
                                 const Roles& aRoles) -> nsAutoString;
};

}  // namespace mozilla

#endif
