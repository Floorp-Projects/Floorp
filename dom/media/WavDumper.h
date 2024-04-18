/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(WavDumper_h_)
#  define WavDumper_h_
#  include <stdio.h>
#  include <stdint.h>
#  include <nsTArray.h>
#  include <nsString.h>
#  include <mozilla/Unused.h>
#  include <mozilla/Atomics.h>
#  include <mozilla/DebugOnly.h>
#  include <mozilla/Sprintf.h>
#  include <ByteWriter.h>

/**
 * If MOZ_DUMP_AUDIO is set, this dumps a file to disk containing the output of
 * an audio stream, in 16bits integers.
 *
 * The sandbox needs to be disabled for this to work.
 */
class WavDumper {
 public:
  WavDumper() = default;
  ~WavDumper() {
    if (mFile) {
      fclose(mFile);
    }
  }

  void Open(const char* aBaseName, uint32_t aChannels, uint32_t aRate) {
    using namespace mozilla;

    if (!getenv("MOZ_DUMP_AUDIO")) {
      return;
    }

    static mozilla::Atomic<int> sDumpedAudioCount(0);

    char buf[100];
    SprintfLiteral(buf, "%s-%d.wav", aBaseName, ++sDumpedAudioCount);
    OpenExplicit(buf, aChannels, aRate);
  }

  void OpenExplicit(const char* aPath, uint32_t aChannels, uint32_t aRate) {
#  ifdef XP_WIN
    nsAutoString widePath = NS_ConvertUTF8toUTF16(aPath);
    mFile = _wfopen(widePath.get(), L"wb");
#  else
    mFile = fopen(aPath, "wb");
#  endif
    if (!mFile) {
      NS_WARNING("Could not open file to DUMP a wav. Is sandboxing disabled?");
      return;
    }
    const uint8_t riffHeader[] = {
        // RIFF header
        0x52, 0x49, 0x46, 0x46, 0x00, 0x00, 0x00, 0x00, 0x57, 0x41, 0x56, 0x45,
        // fmt chunk. We always write 16-bit samples.
        0x66, 0x6d, 0x74, 0x20, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x10, 0x00,
        // data chunk
        0x64, 0x61, 0x74, 0x61, 0xFE, 0xFF, 0xFF, 0x7F};
    AutoTArray<uint8_t, sizeof(riffHeader)> header;
    mozilla::ByteWriter<mozilla::LittleEndian> writer(header);
    static const int CHANNEL_OFFSET = 22;
    static const int SAMPLE_RATE_OFFSET = 24;
    static const int BLOCK_ALIGN_OFFSET = 32;

    mozilla::DebugOnly<bool> rv;
    // Then number of bytes written in each iteration.
    uint32_t written = 0;
    for (size_t i = 0; i != sizeof(riffHeader);) {
      switch (i) {
        case CHANNEL_OFFSET:
          rv = writer.WriteU16(aChannels);
          written = 2;
          MOZ_ASSERT(rv);
          break;
        case SAMPLE_RATE_OFFSET:
          rv = writer.WriteU32(aRate);
          written = 4;
          MOZ_ASSERT(rv);
          break;
        case BLOCK_ALIGN_OFFSET:
          rv = writer.WriteU16(aChannels * 2);
          written = 2;
          MOZ_ASSERT(rv);
          break;
        default:
          // copy from the riffHeader struct above
          rv = writer.WriteU8(riffHeader[i]);
          written = 1;
          MOZ_ASSERT(rv);
          break;
      }
      i += written;
    }
    mozilla::Unused << fwrite(header.Elements(), header.Length(), 1, mFile);
  }

  template <typename T>
  void Write(const T* aBuffer, uint32_t aSamples) {
    if (!mFile) {
      return;
    }
    if (aBuffer) {
      WriteDumpFileHelper(aBuffer, aSamples);
    } else {
      constexpr size_t blockSize = 128;
      T block[blockSize] = {};
      for (size_t remaining = aSamples; remaining;) {
        size_t toWrite = std::min(remaining, blockSize);
        fwrite(block, sizeof(T), toWrite, mFile);
        remaining -= toWrite;
      }
    }
    fflush(mFile);
  }

 private:
  void WriteDumpFileHelper(const int16_t* aInput, size_t aSamples) {
    mozilla::Unused << fwrite(aInput, sizeof(int16_t), aSamples, mFile);
  }

  void WriteDumpFileHelper(const float* aInput, size_t aSamples) {
    using namespace mozilla;

    AutoTArray<uint8_t, 1024 * 2> buf;
    mozilla::ByteWriter<mozilla::LittleEndian> writer(buf);
    for (uint32_t i = 0; i < aSamples; ++i) {
      mozilla::DebugOnly<bool> rv =
          writer.WriteU16(int16_t(aInput[i] * 32767.0f));
      MOZ_ASSERT(rv);
    }
    mozilla::Unused << fwrite(buf.Elements(), buf.Length(), 1, mFile);
  }

  FILE* mFile = nullptr;
};

#endif  // WavDumper_h_
