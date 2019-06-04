/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BinASTTokenReaderContext.h"

#include "mozilla/Result.h"     // MOZ_TRY*
#include "mozilla/ScopeExit.h"  // mozilla::MakeScopeExit

#include <string.h>  // memchr, memcmp, memmove

#include "frontend/BinAST-macros.h"  // BINJS_TRY*, BINJS_MOZ_TRY*
#include "gc/Rooting.h"              // RootedAtom
#include "js/AllocPolicy.h"          // SystemAllocPolicy
#include "js/StableStringChars.h"    // Latin1Char
#include "js/Utility.h"              // js_free
#include "js/Vector.h"               // js::Vector
#include "util/StringBuffer.h"       // StringBuffer
#include "vm/JSAtom.h"               // AtomizeWTF8Chars
#include "vm/JSScript.h"             // ScriptSource

namespace js {
namespace frontend {

// The magic header, at the start of every binjs file.
const char CX_MAGIC_HEADER[] =
    "\x89"
    "BJS\r\n\0\n";

// The latest format version understood by this tokenizer.
const uint32_t MAGIC_FORMAT_VERSION = 2;

using AutoList = BinASTTokenReaderContext::AutoList;
using AutoTaggedTuple = BinASTTokenReaderContext::AutoTaggedTuple;
using CharSlice = BinaryASTSupport::CharSlice;
using Chars = BinASTTokenReaderContext::Chars;

BinASTTokenReaderContext::BinASTTokenReaderContext(JSContext* cx,
                                                   ErrorReporter* er,
                                                   const uint8_t* start,
                                                   const size_t length)
    : BinASTTokenReaderBase(cx, er, start, length),
      metadata_(nullptr),
      posBeforeTree_(nullptr) {
  MOZ_ASSERT(er);
}

BinASTTokenReaderContext::~BinASTTokenReaderContext() {
  if (metadata_ && metadataOwned_ == MetadataOwnership::Owned) {
    UniqueBinASTSourceMetadataPtr ptr(metadata_);
  }
  if (decoder_) {
    BrotliDecoderDestroyInstance(decoder_);
  }
}

template <>
JS::Result<Ok>
BinASTTokenReaderContext::readBuf<BinASTTokenReaderContext::Compression::No>(
    uint8_t* bytes, uint32_t len) {
  return Base::readBuf(bytes, len);
}

template <>
JS::Result<Ok>
BinASTTokenReaderContext::readBuf<BinASTTokenReaderContext::Compression::Yes>(
    uint8_t* bytes, uint32_t len) {
  while (availableDecodedLength() < len) {
    if (availableDecodedLength()) {
      memmove(bytes, decodedBufferBegin(), availableDecodedLength());
      bytes += availableDecodedLength();
      len -= availableDecodedLength();
    }

    MOZ_TRY(fillDecodedBuf());
  }

  memmove(bytes, decodedBufferBegin(), len);
  advanceDecodedBuffer(len);
  return Ok();
}

JS::Result<Ok> BinASTTokenReaderContext::fillDecodedBuf() {
  if (isEOF()) {
    return raiseError("Unexpected end of file");
  }

  MOZ_ASSERT(!availableDecodedLength());

  decodedBegin_ = 0;

  size_t inSize = stop_ - current_;
  size_t outSize = DECODED_BUFFER_SIZE;
  uint8_t* out = decodedBuffer_;

  BrotliDecoderResult result;
  result = BrotliDecoderDecompressStream(decoder_, &inSize, &current_, &outSize,
                                         &out,
                                         /* total_out = */ nullptr);
  if (result == BROTLI_DECODER_RESULT_ERROR) {
    return raiseError("Failed to decompress brotli stream");
  }

  decodedEnd_ = out - decodedBuffer_;

  return Ok();
}

void BinASTTokenReaderContext::advanceDecodedBuffer(uint32_t count) {
  MOZ_ASSERT(decodedBegin_ + count <= decodedEnd_);
  decodedBegin_ += count;
}

bool BinASTTokenReaderContext::isEOF() const {
  return BrotliDecoderIsFinished(decoder_);
}

template <>
JS::Result<uint8_t> BinASTTokenReaderContext::readByte<
    BinASTTokenReaderContext::Compression::No>() {
  return Base::readByte();
}

template <>
JS::Result<uint8_t> BinASTTokenReaderContext::readByte<
    BinASTTokenReaderContext::Compression::Yes>() {
  uint8_t buf;
  MOZ_TRY(readBuf<Compression::Yes>(&buf, 1));
  return buf;
}

BinASTSourceMetadata* BinASTTokenReaderContext::takeMetadata() {
  MOZ_ASSERT(metadataOwned_ == MetadataOwnership::Owned);
  metadataOwned_ = MetadataOwnership::Unowned;
  return metadata_;
}

JS::Result<Ok> BinASTTokenReaderContext::initFromScriptSource(
    ScriptSource* scriptSource) {
  metadata_ = scriptSource->binASTSourceMetadata();
  metadataOwned_ = MetadataOwnership::Unowned;

  return Ok();
}

JS::Result<Ok> BinASTTokenReaderContext::readHeader() {
  // Check that we don't call this function twice.
  MOZ_ASSERT(!posBeforeTree_);

  // Read global headers.
  MOZ_TRY(readConst(CX_MAGIC_HEADER));
  BINJS_MOZ_TRY_DECL(version, readVarU32<Compression::No>());

  if (version != MAGIC_FORMAT_VERSION) {
    return raiseError("Format version not implemented");
  }

  decoder_ = BrotliDecoderCreateInstance(/* alloc_func = */ nullptr,
                                         /* free_func = */ nullptr,
                                         /* opaque = */ nullptr);
  if (!decoder_) {
    return raiseError("Failed to create brotli decoder");
  }

  MOZ_TRY(readStringPrelude());

  // TODO: handle models here.

  return raiseError("Not Yet Implemented");
}

JS::Result<Ok> BinASTTokenReaderContext::readStringPrelude() {
  BINJS_MOZ_TRY_DECL(stringsNumberOfEntries, readVarU32<Compression::Yes>());

  const uint32_t MAX_NUMBER_OF_STRINGS = 32768;

  if (stringsNumberOfEntries > MAX_NUMBER_OF_STRINGS) {
    return raiseError("Too many entries in strings dictionary");
  }

  // FIXME: We don't use the global table like multipart format, but
  //        (interface, field)-local dictionary.
  //        Metadata should be fixed to store that.
  Vector<BinASTKind> binASTKinds(cx_);

  BinASTSourceMetadata* metadata =
      BinASTSourceMetadata::Create(binASTKinds, stringsNumberOfEntries);
  if (!metadata) {
    return raiseOOM();
  }

  // Free it if we don't make it out of here alive. Since we don't want to
  // calloc(), we need to avoid marking atoms that might not be there.
  auto se = mozilla::MakeScopeExit([metadata]() { js_free(metadata); });

  // Below, we read at most DECODED_BUFFER_SIZE bytes at once and look for
  // strings there. We can overrun to the model part, and in that case put
  // unused part back to `decodedBuffer_`.

  // This buffer holds partially-read string, as Latin1Char.  This doesn't match
  // to the actual encoding of the strings that is WTF-8, but the validation
  // should be done later by `AtomizeWTF8Chars`, not immediately.
  StringBuffer buf(cx_);

  RootedAtom atom(cx_);

  for (uint32_t stringIndex = 0; stringIndex < stringsNumberOfEntries;
       stringIndex++) {
    size_t len;
    while (true) {
      if (availableDecodedLength() == 0) {
        MOZ_TRY(fillDecodedBuf());
      }

      MOZ_ASSERT(availableDecodedLength() > 0);

      const uint8_t* end = static_cast<const uint8_t*>(
          memchr(decodedBufferBegin(), '\0', availableDecodedLength()));
      if (end) {
        // Found the end of current string.
        len = end - decodedBufferBegin();
        break;
      }

      BINJS_TRY(buf.append(decodedBufferBegin(), availableDecodedLength()));
      advanceDecodedBuffer(availableDecodedLength());
    }

    // FIXME: handle 0x01-escaped string here.

    if (!buf.length()) {
      // If there's no partial string in the buffer, bypass it.
      const char* begin = reinterpret_cast<const char*>(decodedBufferBegin());
      BINJS_TRY_VAR(atom, AtomizeWTF8Chars(cx_, begin, len));
    } else {
      BINJS_TRY(
          buf.append(reinterpret_cast<const char*>(decodedBufferBegin()), len));

      const char* begin = reinterpret_cast<const char*>(buf.rawLatin1Begin());
      len = buf.length();

      // We cannot use `StringBuffer::finishAtom` because the string is WTF-8.
      BINJS_TRY_VAR(atom, AtomizeWTF8Chars(cx_, begin, len));

      buf.clear();
    }

    // FIXME: We don't populate `slicesTable_` given we won't use it.
    //        Update BinASTSourceMetadata to reflect this.

    metadata->getAtom(stringIndex) = atom;

    // +1 for skipping 0x00.
    advanceDecodedBuffer(len + 1);
  }

  // Found all strings. The remaining data is kept in buffer and will be
  // used for later read.

  MOZ_ASSERT(!metadata_);
  MOZ_ASSERT(buf.empty());
  se.release();
  metadata_ = metadata;
  metadataOwned_ = MetadataOwnership::Owned;

  return Ok();
}

void BinASTTokenReaderContext::traceMetadata(JSTracer* trc) {
  if (metadata_) {
    metadata_->trace(trc);
  }
}

JS::Result<bool> BinASTTokenReaderContext::readBool(const Context&) {
  return raiseError("Not Yet Implemented");
}

JS::Result<double> BinASTTokenReaderContext::readDouble(const Context&) {
  return raiseError("Not Yet Implemented");
}

JS::Result<JSAtom*> BinASTTokenReaderContext::readMaybeAtom(const Context&) {
  return raiseError("Not Yet Implemented");
}

JS::Result<JSAtom*> BinASTTokenReaderContext::readAtom(const Context&) {
  return raiseError("Not Yet Implemented");
}

JS::Result<JSAtom*> BinASTTokenReaderContext::readMaybeIdentifierName(
    const Context&) {
  return raiseError("Not Yet Implemented");
}

JS::Result<JSAtom*> BinASTTokenReaderContext::readIdentifierName(
    const Context&) {
  return raiseError("Not Yet Implemented");
}

JS::Result<JSAtom*> BinASTTokenReaderContext::readPropertyKey(const Context&) {
  return raiseError("Not Yet Implemented");
}

JS::Result<Ok> BinASTTokenReaderContext::readChars(Chars& out, const Context&) {
  return raiseError("Not Yet Implemented");
}

JS::Result<BinASTVariant> BinASTTokenReaderContext::readVariant(
    const Context&) {
  return raiseError("Not Yet Implemented");
}

JS::Result<BinASTTokenReaderBase::SkippableSubTree>
BinASTTokenReaderContext::readSkippableSubTree(const Context&) {
  return raiseError("Not Yet Implemented");
}

JS::Result<Ok> BinASTTokenReaderContext::enterTaggedTuple(
    BinASTKind& tag, BinASTTokenReaderContext::BinASTFields&, const Context&,
    AutoTaggedTuple& guard) {
  return raiseError("Not Yet Implemented");
}

JS::Result<Ok> BinASTTokenReaderContext::enterList(uint32_t& items,
                                                   const Context&,
                                                   AutoList& guard) {
  return raiseError("Not Yet Implemented");
}

void BinASTTokenReaderContext::AutoBase::init() { initialized_ = true; }

BinASTTokenReaderContext::AutoBase::AutoBase(BinASTTokenReaderContext& reader)
    : initialized_(false), reader_(reader) {}

BinASTTokenReaderContext::AutoBase::~AutoBase() {
  // By now, the `AutoBase` must have been deinitialized by calling `done()`.
  // The only case in which we can accept not calling `done()` is if we have
  // bailed out because of an error.
  MOZ_ASSERT_IF(initialized_, reader_.hasRaisedError());
}

JS::Result<Ok> BinASTTokenReaderContext::AutoBase::checkPosition(
    const uint8_t* expectedEnd) {
  return reader_.raiseError("Not Yet Implemented");
}

BinASTTokenReaderContext::AutoList::AutoList(BinASTTokenReaderContext& reader)
    : AutoBase(reader) {}

void BinASTTokenReaderContext::AutoList::init() { AutoBase::init(); }

JS::Result<Ok> BinASTTokenReaderContext::AutoList::done() {
  return reader_.raiseError("Not Yet Implemented");
}

// Internal uint32_t
// Note that this is different than varnum in multipart.
//
// Encoded as variable length number.

template <BinASTTokenReaderContext::Compression compression>
JS::Result<uint32_t> BinASTTokenReaderContext::readVarU32() {
  uint32_t result = 0;
  uint32_t shift = 0;
  while (true) {
    MOZ_ASSERT(shift < 32);
    uint32_t byte;
    MOZ_TRY_VAR(byte, readByte<compression>());

    const uint32_t newResult = result | (byte & 0x7f) << shift;
    if (newResult < result) {
      return raiseError("Overflow during readVarU32");
    }

    result = newResult;
    shift += 7;

    if ((byte & 0x80) == 0) {
      return result;
    }

    if (shift >= 32) {
      return raiseError("Overflow during readVarU32");
    }
  }
}

JS::Result<uint32_t> BinASTTokenReaderContext::readUnsignedLong(
    const Context&) {
  return readVarU32<Compression::Yes>();
}

BinASTTokenReaderContext::AutoTaggedTuple::AutoTaggedTuple(
    BinASTTokenReaderContext& reader)
    : AutoBase(reader) {}

JS::Result<Ok> BinASTTokenReaderContext::AutoTaggedTuple::done() {
  return reader_.raiseError("Not Yet Implemented");
}

}  // namespace frontend

}  // namespace js
