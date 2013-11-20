// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DECRYPT_CONFIG_H_
#define MEDIA_BASE_DECRYPT_CONFIG_H_

#include <string>
#include <vector>

#include "mp4_demuxer/basictypes.h"

namespace mp4_demuxer {

// The Common Encryption spec provides for subsample encryption, where portions
// of a sample are set in cleartext. A SubsampleEntry specifies the number of
// clear and encrypted bytes in each subsample. For decryption, all of the
// encrypted bytes in a sample should be considered a single logical stream,
// regardless of how they are divided into subsamples, and the clear bytes
// should not be considered as part of decryption. This is logically equivalent
// to concatenating all 'cypher_bytes' portions of subsamples, decrypting that
// result, and then copying each byte from the decrypted block over the
// position of the corresponding encrypted byte.
struct SubsampleEntry {
  uint32_t clear_bytes;
  uint32_t cypher_bytes;
};

// Contains all information that a decryptor needs to decrypt a media sample.
class DecryptConfig {
 public:
  // Keys are always 128 bits.
  static const int kDecryptionKeySize = 16;

  // |key_id| is the ID that references the decryption key for this sample.
  // |iv| is the initialization vector defined by the encrypted format.
  //   Currently |iv| must be 16 bytes as defined by WebM and ISO. Or must be
  //   empty which signals an unencrypted frame.
  // |data_offset| is the amount of data that should be discarded from the
  //   head of the sample buffer before applying subsample information. A
  //   decrypted buffer will be shorter than an encrypted buffer by this amount.
  // |subsamples| defines the clear and encrypted portions of the sample as
  //   described above. A decrypted buffer will be equal in size to the sum
  //   of the subsample sizes.
  //
  // |data_offset| is applied before |subsamples|.
  DecryptConfig(const std::string& key_id,
                const std::string& iv,
                const int data_offset,
                const std::vector<SubsampleEntry>& subsamples);
  ~DecryptConfig();

  const std::string& key_id() const { return key_id_; }
  const std::string& iv() const { return iv_; }
  int data_offset() const { return data_offset_; }
  const std::vector<SubsampleEntry>& subsamples() const { return subsamples_; }

 private:
  const std::string key_id_;

  // Initialization vector.
  const std::string iv_;

  // TODO(fgalligan): Remove |data_offset_| if there is no plan to use it in
  // the future.
  // Amount of data to be discarded before applying subsample information.
  const int data_offset_;

  // Subsample information. May be empty for some formats, meaning entire frame
  // (less data ignored by data_offset_) is encrypted.
  const std::vector<SubsampleEntry> subsamples_;

  DISALLOW_COPY_AND_ASSIGN(DecryptConfig);
};

}  // namespace mp4_demuxer

#endif  // MEDIA_BASE_DECRYPT_CONFIG_H_
