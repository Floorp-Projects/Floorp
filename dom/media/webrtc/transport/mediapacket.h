/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mediapacket_h__
#define mediapacket_h__

#include <cstddef>
#include <cstdint>
#include "mozilla/UniquePtr.h"
#include "mozilla/Maybe.h"

class PickleIterator;

namespace IPC {
class Message;
class MessageReader;
class MessageWriter;
}  // namespace IPC

namespace mozilla {

// TODO: It might be worthwhile to teach this class how to "borrow" a buffer.
// That would make it easier to misuse, however, so maybe not worth it.
class MediaPacket {
 public:
  MediaPacket() = default;
  MediaPacket(MediaPacket&& orig) = default;
  MediaPacket& operator=(MediaPacket&& orig) = default;

  MediaPacket Clone() const;

  // Takes ownership of the passed-in data
  void Take(UniquePtr<uint8_t[]>&& data, size_t len, size_t capacity = 0) {
    data_ = std::move(data);
    len_ = len;
    if (capacity < len) {
      capacity = len;
    }
    capacity_ = capacity;
  }

  void Reset() {
    data_.reset();
    len_ = 0;
    capacity_ = 0;
    encrypted_data_.reset();
    encrypted_len_ = 0;
    sdp_level_.reset();
  }

  // Copies the passed-in data
  void Copy(const uint8_t* data, size_t len, size_t capacity = 0);

  uint8_t* data() const { return data_.get(); }

  size_t len() const { return len_; }

  void SetLength(size_t length) { len_ = length; }

  size_t capacity() const { return capacity_; }

  Maybe<size_t>& sdp_level() { return sdp_level_; }

  void CopyDataToEncrypted() {
    encrypted_data_ = std::move(data_);
    encrypted_len_ = len_;
    Copy(encrypted_data_.get(), len_);
  }

  const uint8_t* encrypted_data() const { return encrypted_data_.get(); }

  size_t encrypted_len() const { return encrypted_len_; }

  enum Type { UNCLASSIFIED, SRTP, SRTCP, DTLS, RTP, RTCP, SCTP };

  void Categorize();

  void SetType(Type type) { type_ = type; }

  Type type() const { return type_; }

  void Serialize(IPC::MessageWriter* aWriter) const;
  bool Deserialize(IPC::MessageReader* aReader);

 private:
  UniquePtr<uint8_t[]> data_;
  size_t len_ = 0;
  size_t capacity_ = 0;
  // Encrypted form of the data, if there is one.
  UniquePtr<uint8_t[]> encrypted_data_;
  size_t encrypted_len_ = 0;
  // SDP level that this packet belongs to, if known.
  Maybe<size_t> sdp_level_;
  Type type_ = UNCLASSIFIED;
};
}  // namespace mozilla

namespace IPC {
template <typename>
struct ParamTraits;

template <>
struct ParamTraits<mozilla::MediaPacket> {
  static void Write(MessageWriter* aWriter,
                    const mozilla::MediaPacket& aParam) {
    aParam.Serialize(aWriter);
  }

  static bool Read(MessageReader* aReader, mozilla::MediaPacket* aResult) {
    return aResult->Deserialize(aReader);
  }
};
}  // namespace IPC
#endif  // mediapacket_h__
