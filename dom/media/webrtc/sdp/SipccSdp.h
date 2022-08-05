/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _SIPCCSDP_H_
#define _SIPCCSDP_H_

#include <map>
#include <vector>
#include "mozilla/Attributes.h"

#include "sdp/Sdp.h"
#include "sdp/SdpParser.h"
#include "sdp/SipccSdpMediaSection.h"
#include "sdp/SipccSdpAttributeList.h"
extern "C" {
#include "sipcc_sdp.h"
}

namespace mozilla {

class SipccSdpParser;

class SipccSdp final : public Sdp {
  friend class SipccSdpParser;

 public:
  explicit SipccSdp(const SdpOrigin& origin)
      : mOrigin(origin), mAttributeList(nullptr) {}
  SipccSdp(const SipccSdp& aOrig);

  virtual Sdp* Clone() const override;

  virtual const SdpOrigin& GetOrigin() const override;

  // Note: connection information is always retrieved from media sections
  virtual uint32_t GetBandwidth(const std::string& type) const override;

  virtual size_t GetMediaSectionCount() const override {
    return mMediaSections.size();
  }

  virtual const SdpAttributeList& GetAttributeList() const override {
    return mAttributeList;
  }

  virtual SdpAttributeList& GetAttributeList() override {
    return mAttributeList;
  }

  virtual const SdpMediaSection& GetMediaSection(size_t level) const override;

  virtual SdpMediaSection& GetMediaSection(size_t level) override;

  virtual SdpMediaSection& AddMediaSection(SdpMediaSection::MediaType media,
                                           SdpDirectionAttribute::Direction dir,
                                           uint16_t port,
                                           SdpMediaSection::Protocol proto,
                                           sdp::AddrType addrType,
                                           const std::string& addr) override;

  virtual void Serialize(std::ostream&) const override;

 private:
  using InternalResults = SdpParser::InternalResults;

  SipccSdp() : mOrigin("", 0, 0, sdp::kIPv4, ""), mAttributeList(nullptr) {}

  bool Load(sdp_t* sdp, InternalResults& results);
  bool LoadOrigin(sdp_t* sdp, InternalResults& results);

  SdpOrigin mOrigin;
  SipccSdpBandwidths mBandwidths;
  SipccSdpAttributeList mAttributeList;
  std::vector<UniquePtr<SipccSdpMediaSection>> mMediaSections;
};

}  // namespace mozilla

#endif  // _sdp_h_
