/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sdp/SipccSdpParser.h"
#include "sdp/SipccSdp.h"

#include <utility>
extern "C" {
#include "sipcc_sdp.h"
}

namespace mozilla {

extern "C" {

void sipcc_sdp_parser_results_handler(void* context, uint32_t line,
                                      const char* message) {
  auto* results = static_cast<UniquePtr<InternalResults>*>(context);
  std::string err(message);
  (*results)->AddParseError(line, err);
}

}  // extern "C"

const std::string& SipccSdpParser::ParserName() {
  static const std::string SIPCC_NAME = "SIPCC";
  return SIPCC_NAME;
}

UniquePtr<SdpParser::Results> SipccSdpParser::Parse(const std::string& aText) {
  UniquePtr<InternalResults> results(new InternalResults(Name()));
  sdp_conf_options_t* sipcc_config = sdp_init_config();
  if (!sipcc_config) {
    return UniquePtr<SdpParser::Results>();
  }

  sdp_nettype_supported(sipcc_config, SDP_NT_INTERNET, true);
  sdp_addrtype_supported(sipcc_config, SDP_AT_IP4, true);
  sdp_addrtype_supported(sipcc_config, SDP_AT_IP6, true);
  sdp_transport_supported(sipcc_config, SDP_TRANSPORT_RTPAVP, true);
  sdp_transport_supported(sipcc_config, SDP_TRANSPORT_RTPAVPF, true);
  sdp_transport_supported(sipcc_config, SDP_TRANSPORT_RTPSAVP, true);
  sdp_transport_supported(sipcc_config, SDP_TRANSPORT_RTPSAVPF, true);
  sdp_transport_supported(sipcc_config, SDP_TRANSPORT_UDPTLSRTPSAVP, true);
  sdp_transport_supported(sipcc_config, SDP_TRANSPORT_UDPTLSRTPSAVPF, true);
  sdp_transport_supported(sipcc_config, SDP_TRANSPORT_TCPDTLSRTPSAVP, true);
  sdp_transport_supported(sipcc_config, SDP_TRANSPORT_TCPDTLSRTPSAVPF, true);
  sdp_transport_supported(sipcc_config, SDP_TRANSPORT_DTLSSCTP, true);
  sdp_transport_supported(sipcc_config, SDP_TRANSPORT_UDPDTLSSCTP, true);
  sdp_transport_supported(sipcc_config, SDP_TRANSPORT_TCPDTLSSCTP, true);
  sdp_require_session_name(sipcc_config, false);

  sdp_config_set_error_handler(sipcc_config, &sipcc_sdp_parser_results_handler,
                               &results);

  // Takes ownership of |sipcc_config| iff it succeeds
  sdp_t* sdp = sdp_init_description(sipcc_config);
  if (!sdp) {
    sdp_free_config(sipcc_config);
    return results;
  }

  const char* rawString = aText.c_str();
  sdp_result_e sdpres = sdp_parse(sdp, rawString, aText.length());
  if (sdpres != SDP_SUCCESS) {
    sdp_free_description(sdp);
    return results;
  }

  UniquePtr<SipccSdp> sipccSdp(new SipccSdp);

  bool success = sipccSdp->Load(sdp, *results);
  sdp_free_description(sdp);
  if (success) {
    results->SetSdp(UniquePtr<mozilla::Sdp>(std::move(sipccSdp)));
  }

  return results;
}

bool SipccSdpParser::IsNamed(const std::string& aName) {
  return aName == ParserName();
}

}  // namespace mozilla
