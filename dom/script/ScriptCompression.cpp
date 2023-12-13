/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "zlib.h"
#include "ScriptLoadRequest.h"
#include "ScriptLoader.h"
#include "mozilla/PerfStats.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/Vector.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_browser.h"

using namespace mozilla;

namespace JS::loader {

#undef LOG
#define LOG(args)                                       \
  MOZ_LOG(mozilla::dom::ScriptLoader::gScriptLoaderLog, \
          mozilla::LogLevel::Debug, args)

/*
 * ScriptBytecodeDataLayout
 *
 * ScriptBytecodeDataLayout provides accessors to maintain the correct data
 * layout of the ScriptLoadRequest's script bytecode buffer.
 */
class ScriptBytecodeDataLayout {
 public:
  explicit ScriptBytecodeDataLayout(mozilla::Vector<uint8_t>& aBytecode,
                                    size_t aBytecodeOffset)
      : mBytecode(aBytecode), mBytecodeOffset(aBytecodeOffset) {}

  uint8_t* prelude() const { return mBytecode.begin(); }
  size_t preludeLength() const { return mBytecodeOffset; }

  uint8_t* bytecode() const { return prelude() + mBytecodeOffset; }
  size_t bytecodeLength() const { return mBytecode.length() - preludeLength(); }

  mozilla::Vector<uint8_t>& mBytecode;
  size_t mBytecodeOffset;
};

/*
 * ScriptBytecodeCompressedDataLayout
 *
 * ScriptBytecodeCompressedDataLayout provides accessors to maintain the correct
 * data layout of a compressed script bytecode buffer.
 */
class ScriptBytecodeCompressedDataLayout {
 public:
  using UncompressedLengthType = uint32_t;

  explicit ScriptBytecodeCompressedDataLayout(
      mozilla::Vector<uint8_t>& aBytecode, size_t aBytecodeOffset)
      : mBytecode(aBytecode), mBytecodeOffset(aBytecodeOffset) {}

  uint8_t* prelude() const { return mBytecode.begin(); }
  size_t preludeLength() const { return mBytecodeOffset; }

  uint8_t* uncompressedLength() const { return prelude() + mBytecodeOffset; }
  size_t uncompressedLengthLength() const {
    return sizeof(UncompressedLengthType);
  }

  uint8_t* bytecode() const {
    return uncompressedLength() + uncompressedLengthLength();
  }
  size_t bytecodeLength() const {
    return mBytecode.length() - uncompressedLengthLength() - preludeLength();
  }

  mozilla::Vector<uint8_t>& mBytecode;
  size_t mBytecodeOffset;
};

bool ScriptBytecodeCompress(Vector<uint8_t>& aBytecodeBuf,
                            size_t aBytecodeOffset,
                            Vector<uint8_t>& aCompressedBytecodeBufOut) {
  // TODO probably need to move this to a helper thread

  AUTO_PROFILER_MARKER_TEXT("ScriptBytecodeCompress", JS, {}, ""_ns);
  PerfStats::AutoMetricRecording<PerfStats::Metric::JSBC_Compression>
      autoRecording;

  ScriptBytecodeDataLayout uncompressedLayout(aBytecodeBuf, aBytecodeOffset);
  ScriptBytecodeCompressedDataLayout compressedLayout(
      aCompressedBytecodeBufOut, uncompressedLayout.preludeLength());
  ScriptBytecodeCompressedDataLayout::UncompressedLengthType
      uncompressedLength = uncompressedLayout.bytecodeLength();
  z_stream zstream{.next_in = uncompressedLayout.bytecode(),
                   .avail_in = uncompressedLength};

  // Note: deflateInit needs to be called before deflateBound.
  const uint32_t compressionLevel =
      StaticPrefs::browser_cache_jsbc_compression_level();
  if (deflateInit(&zstream, compressionLevel) != Z_OK) {
    LOG(
        ("ScriptLoadRequest: Unable to initialize bytecode cache "
         "compression."));
    return false;
  }
  auto autoDestroy = MakeScopeExit([&]() { deflateEnd(&zstream); });

  auto compressedLength = deflateBound(&zstream, uncompressedLength);
  if (!aCompressedBytecodeBufOut.resizeUninitialized(
          compressedLength + compressedLayout.preludeLength() +
          compressedLayout.uncompressedLengthLength())) {
    return false;
  }
  memcpy(compressedLayout.prelude(), uncompressedLayout.prelude(),
         uncompressedLayout.preludeLength());
  memcpy(compressedLayout.uncompressedLength(), &uncompressedLength,
         sizeof(uncompressedLength));
  zstream.next_out = compressedLayout.bytecode();
  zstream.avail_out = compressedLength;

  int ret = deflate(&zstream, Z_FINISH);
  if (ret == Z_MEM_ERROR) {
    return false;
  }
  MOZ_RELEASE_ASSERT(ret == Z_STREAM_END);

  aCompressedBytecodeBufOut.shrinkTo(zstream.next_out -
                                     aCompressedBytecodeBufOut.begin());
  return true;
}

bool ScriptBytecodeDecompress(Vector<uint8_t>& aCompressedBytecodeBuf,
                              size_t aBytecodeOffset,
                              Vector<uint8_t>& aBytecodeBufOut) {
  AUTO_PROFILER_MARKER_TEXT("ScriptBytecodeDecompress", JS, {}, ""_ns);
  PerfStats::AutoMetricRecording<PerfStats::Metric::JSBC_Decompression>
      autoRecording;

  ScriptBytecodeDataLayout uncompressedLayout(aBytecodeBufOut, aBytecodeOffset);
  ScriptBytecodeCompressedDataLayout compressedLayout(
      aCompressedBytecodeBuf, uncompressedLayout.preludeLength());
  ScriptBytecodeCompressedDataLayout::UncompressedLengthType uncompressedLength;
  memcpy(&uncompressedLength, compressedLayout.uncompressedLength(),
         compressedLayout.uncompressedLengthLength());
  if (!aBytecodeBufOut.resizeUninitialized(uncompressedLayout.preludeLength() +
                                           uncompressedLength)) {
    return false;
  }
  memcpy(uncompressedLayout.prelude(), compressedLayout.prelude(),
         compressedLayout.preludeLength());

  z_stream zstream{nullptr};
  zstream.next_in = compressedLayout.bytecode();
  zstream.avail_in = static_cast<uint32_t>(compressedLayout.bytecodeLength());
  zstream.next_out = uncompressedLayout.bytecode();
  zstream.avail_out = uncompressedLength;
  if (inflateInit(&zstream) != Z_OK) {
    LOG(("ScriptLoadRequest: inflateInit FAILED (%s)", zstream.msg));
    return false;
  }
  auto autoDestroy = MakeScopeExit([&]() { inflateEnd(&zstream); });

  int ret = inflate(&zstream, Z_NO_FLUSH);
  bool ok = (ret == Z_OK || ret == Z_STREAM_END) && zstream.avail_in == 0;
  if (!ok) {
    LOG(("ScriptLoadReques: inflate FAILED (%s)", zstream.msg));
    return false;
  }

  return true;
}

#undef LOG

}  // namespace JS::loader
