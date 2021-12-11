/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sdp/SdpPref.h"
#include "sdp/RsdparsaSdpParser.h"
#include "sdp/SipccSdpParser.h"

namespace mozilla {

const std::string SdpPref::PRIMARY_PREF = "media.peerconnection.sdp.parser";
const std::string SdpPref::ALTERNATE_PREF =
    "media.peerconnection.sdp.alternate_parse_mode";
const std::string SdpPref::STRICT_SUCCESS_PREF =
    "media.peerconnection.sdp.strict_success";
const std::string SdpPref::DEFAULT = "default";

auto SdpPref::ToString(const Parsers& aParser) -> std::string {
  switch (aParser) {
    case Parsers::Sipcc:
      return "sipcc";
    case Parsers::WebRtcSdp:
      return "webrtc-sdp";
  };
  MOZ_CRASH("ALL Parsers CASES ARE NOT COVERED");
  return "";
}

auto SdpPref::ToString(const AlternateParseModes& aMode) -> std::string {
  switch (aMode) {
    case AlternateParseModes::Parallel:
      return "parallel";
    case AlternateParseModes::Failover:
      return "failover";
    case AlternateParseModes::Never:
      return "never";
  };
  MOZ_CRASH("ALL AlternateParseModes CASES ARE NOT COVERED");
  return "";
}

auto SdpPref::Parser() -> Parsers {
  static const auto values = std::unordered_map<std::string, Parsers>{
      {"sipcc", Parsers::Sipcc},
      {"webrtc-sdp", Parsers::WebRtcSdp},
      {DEFAULT, Parsers::Sipcc},
  };
  return Pref(PRIMARY_PREF, values);
}

auto SdpPref::AlternateParseMode() -> AlternateParseModes {
  static const auto values =
      std::unordered_map<std::string, AlternateParseModes>{
          {"parallel", AlternateParseModes::Parallel},
          {"failover", AlternateParseModes::Failover},
          {"never", AlternateParseModes::Never},
          {DEFAULT, AlternateParseModes::Parallel},
      };
  return Pref(ALTERNATE_PREF, values);
}

auto SdpPref::Primary() -> UniquePtr<SdpParser> {
  switch (Parser()) {
    case Parsers::Sipcc:
      return UniquePtr<SdpParser>(new SipccSdpParser());
    case Parsers::WebRtcSdp:
      return UniquePtr<SdpParser>(new RsdparsaSdpParser());
  }
  MOZ_CRASH("ALL Parsers CASES ARE NOT COVERED");
  return nullptr;
}

auto SdpPref::Secondary() -> Maybe<UniquePtr<SdpParser>> {
  if (AlternateParseMode() != AlternateParseModes::Parallel) {
    return Nothing();
  }
  switch (Parser()) {  // Choose whatever the primary parser isn't
    case Parsers::Sipcc:
      return Some(UniquePtr<SdpParser>(new RsdparsaSdpParser()));
    case Parsers::WebRtcSdp:
      return Some(UniquePtr<SdpParser>(new SipccSdpParser()));
  }
  MOZ_CRASH("ALL Parsers CASES ARE NOT COVERED");
  return Nothing();
}

auto SdpPref::Failover() -> Maybe<UniquePtr<SdpParser>> {
  if (AlternateParseMode() != AlternateParseModes::Failover) {
    return Nothing();
  }
  switch (Parser()) {
    case Parsers::Sipcc:
      return Some(UniquePtr<SdpParser>(new RsdparsaSdpParser()));
    case Parsers::WebRtcSdp:
      return Some(UniquePtr<SdpParser>(new SipccSdpParser()));
  }
  MOZ_CRASH("ALL Parsers CASES ARE NOT COVERED");
  return Nothing();
}

auto SdpPref::StrictSuccess() -> bool {
  return Preferences::GetBool(STRICT_SUCCESS_PREF.c_str(), false);
}

}  // namespace mozilla
