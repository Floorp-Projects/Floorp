/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DECODER_DATA_H_
#define DECODER_DATA_H_

#include "MediaInfo.h"
#include "MediaResult.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Result.h"
#include "mozilla/Types.h"
#include "mozilla/Vector.h"
#include "nsString.h"
#include "nsTArray.h"
#include "mp4parse.h"

namespace mozilla {

class IndiceWrapper;
class MP4Demuxer;

struct PsshInfo {
  PsshInfo() = default;
  PsshInfo(const PsshInfo& aOther) = delete;
  PsshInfo(PsshInfo&& aOther) = default;

  nsTArray<uint8_t> uuid;
  nsTArray<uint8_t> data;

  bool operator==(const PsshInfo& aOther) const {
    return uuid == aOther.uuid && data == aOther.data;
  }
};

class CryptoFile {
 public:
  CryptoFile() : valid(false) {}
  CryptoFile(const CryptoFile& aCryptoFile) = delete;

  void Update(const uint8_t* aData, size_t aLength) {
    valid = DoUpdate(aData, aLength).isOk();
  }

  bool valid;
  nsTArray<PsshInfo> pssh;

 private:
  mozilla::Result<mozilla::Ok, nsresult> DoUpdate(const uint8_t* aData,
                                                  size_t aLength);
};

class MP4AudioInfo : public mozilla::AudioInfo {
 public:
  MP4AudioInfo() = default;

  MediaResult Update(const Mp4parseTrackInfo* aTrack,
                     const Mp4parseTrackAudioInfo* aAudio,
                     const IndiceWrapper* aIndices);

  virtual bool IsValid() const override;
};

class MP4VideoInfo : public mozilla::VideoInfo {
 public:
  MP4VideoInfo() = default;

  MediaResult Update(const Mp4parseTrackInfo* track,
                     const Mp4parseTrackVideoInfo* video);

  virtual bool IsValid() const override;
};

}  // namespace mozilla

#endif
