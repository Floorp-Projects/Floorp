/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sdp/SdpTelemetry.h"

#include "mozilla/Telemetry.h"

namespace mozilla {

auto SdpTelemetry::RecordParse(const SdpTelemetry::Results& aResult,
                               const SdpTelemetry::Modes& aMode,
                               const SdpTelemetry::Roles& aRole) -> void {
  Telemetry::ScalarAdd(Telemetry::ScalarID::WEBRTC_SDP_PARSER_DIFF,
                       BucketNameFragment(aResult, aMode, aRole), 1);
}

auto SdpTelemetry::RecordCompare(const SdpTelemetry::Results& aFirst,
                                 const SdpTelemetry::Results& aSecond,
                                 const SdpTelemetry::Modes& aMode) -> void {
  const nsAutoString bucket =
      BucketNameFragment(aFirst, aMode, Roles::Primary) +
      NS_ConvertASCIItoUTF16("__") +
      BucketNameFragment(aSecond, aMode, Roles::Secondary);
  Telemetry::ScalarAdd(Telemetry::ScalarID::WEBRTC_SDP_PARSER_DIFF, bucket, 1);
}

auto SdpTelemetry::BucketNameFragment(const SdpTelemetry::Results& aResult,
                                      const SdpTelemetry::Modes& aMode,
                                      const SdpTelemetry::Roles& aRole)
    -> nsAutoString {
  auto mode = [&]() -> std::string {
    switch (aMode) {
      case Modes::Parallel:
        return "parallel";
      case Modes::Failover:
        return "failover";
      case Modes::Never:
        return "standalone";
    }
    MOZ_CRASH("Unknown SDP Parse Mode!");
  };
  auto role = [&]() -> std::string {
    switch (aRole) {
      case Roles::Primary:
        return "primary";
      case Roles::Secondary:
        return "secondary";
    }
    MOZ_CRASH("Unknown SDP Parse Role!");
  };
  auto success = [&]() -> std::string {
    return aResult->Ok() ? "success" : "failure";
  };
  nsAutoString name;
  name.AssignASCII(nsCString(aResult->ParserName() + "_" + mode() + "_" +
                             role() + "_" + success()));
  return name;
}

}  // namespace mozilla
