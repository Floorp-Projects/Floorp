/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mediapacket.h"

#include <cstring>
#include "ipc/IPCMessageUtils.h"

namespace mozilla {

void MediaPacket::Copy(const uint8_t* data, size_t len, size_t capacity) {
  if (capacity < len) {
    capacity = len;
  }
  data_.reset(new uint8_t[capacity]);
  len_ = len;
  capacity_ = capacity;
  memcpy(data_.get(), data, len);
}

MediaPacket MediaPacket::Clone() const {
  MediaPacket newPacket;
  newPacket.type_ = type_;
  newPacket.sdp_level_ = sdp_level_;
  newPacket.Copy(data_.get(), len_, capacity_);
  return newPacket;
}

void MediaPacket::Serialize(IPC::MessageWriter* aWriter) const {
  aWriter->WriteUInt32(len_);
  aWriter->WriteUInt32(capacity_);
  if (len_) {
    aWriter->WriteBytes(data_.get(), len_);
  }
  aWriter->WriteUInt32(encrypted_len_);
  if (encrypted_len_) {
    aWriter->WriteBytes(encrypted_data_.get(), encrypted_len_);
  }
  aWriter->WriteInt32(sdp_level_.isSome() ? *sdp_level_ : -1);
  aWriter->WriteInt32(type_);
}

bool MediaPacket::Deserialize(IPC::MessageReader* aReader) {
  Reset();
  uint32_t len;
  if (!aReader->ReadUInt32(&len)) {
    return false;
  }
  uint32_t capacity;
  if (!aReader->ReadUInt32(&capacity)) {
    return false;
  }
  if (len) {
    MOZ_RELEASE_ASSERT(capacity >= len);
    UniquePtr<uint8_t[]> data(new uint8_t[capacity]);
    if (!aReader->ReadBytesInto(data.get(), len)) {
      return false;
    }
    data_ = std::move(data);
    len_ = len;
    capacity_ = capacity;
  }

  if (!aReader->ReadUInt32(&len)) {
    return false;
  }
  if (len) {
    UniquePtr<uint8_t[]> data(new uint8_t[len]);
    if (!aReader->ReadBytesInto(data.get(), len)) {
      return false;
    }
    encrypted_data_ = std::move(data);
    encrypted_len_ = len;
  }

  int32_t sdp_level;
  if (!aReader->ReadInt32(&sdp_level)) {
    return false;
  }

  if (sdp_level >= 0) {
    sdp_level_ = Some(sdp_level);
  }

  int32_t type;
  if (!aReader->ReadInt32(&type)) {
    return false;
  }
  type_ = static_cast<Type>(type);
  return true;
}

static bool IsRtp(const uint8_t* data, size_t len) {
  if (len < 2) return false;

  // Check if this is a RTCP packet. Logic based on the types listed in
  // media/webrtc/trunk/src/modules/rtp_rtcp/source/rtp_utility.cc

  // Anything outside this range is RTP.
  if ((data[1] < 192) || (data[1] > 207)) return true;

  if (data[1] == 192)  // FIR
    return false;

  if (data[1] == 193)  // NACK, but could also be RTP. This makes us sad
    return true;       // but it's how webrtc.org behaves.

  if (data[1] == 194) return true;

  if (data[1] == 195)  // IJ.
    return false;

  if ((data[1] > 195) && (data[1] < 200))  // the > 195 is redundant
    return true;

  if ((data[1] >= 200) && (data[1] <= 207))  // SR, RR, SDES, BYE,
    return false;                            // APP, RTPFB, PSFB, XR

  MOZ_ASSERT(false);  // Not reached, belt and suspenders.
  return true;
}

void MediaPacket::Categorize() {
  SetType(MediaPacket::UNCLASSIFIED);

  if (!data_ || len_ < 4) {
    return;
  }

  if (data_[0] >= 20 && data_[0] <= 63) {
    // DTLS per RFC 7983
    SetType(MediaPacket::DTLS);
  } else if (data_[0] > 127 && data_[0] < 192) {
    // RTP/RTCP per RFC 7983
    if (IsRtp(data_.get(), len_)) {
      SetType(MediaPacket::SRTP);
    } else {
      SetType(MediaPacket::SRTCP);
    }
  }
}
}  // namespace mozilla
