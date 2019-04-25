/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BinASTTokenReaderMultipart.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Casting.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/Maybe.h"
#include "mozilla/ScopeExit.h"

#include <utility>

#include "frontend/BinAST-macros.h"
#include "frontend/BinASTRuntimeSupport.h"
#include "frontend/BytecodeCompiler.h"  // IsIdentifier

#include "js/Result.h"

namespace js {
namespace frontend {

// The magic header, at the start of every binjs file.
const char MAGIC_HEADER[] = "BINJS";
// The latest format version understood by this tokenizer.
const uint32_t MAGIC_FORMAT_VERSION = 1;

// The headers at the start of each section of the binjs file.
const char SECTION_HEADER_GRAMMAR[] = "[GRAMMAR]";
const char SECTION_HEADER_STRINGS[] = "[STRINGS]";
const char SECTION_HEADER_TREE[] = "[TREE]";

// The (only) internal compression mechanism understood by this parser.
const char COMPRESSION_IDENTITY[] = "identity;";

// The maximal number of distinct strings that may be declared in a
// single file.
const uint32_t MAX_NUMBER_OF_STRINGS = 32768;

using AutoList = BinASTTokenReaderMultipart::AutoList;
using AutoTaggedTuple = BinASTTokenReaderMultipart::AutoTaggedTuple;
using CharSlice = BinaryASTSupport::CharSlice;
using Chars = BinASTTokenReaderMultipart::Chars;

BinASTTokenReaderMultipart::BinASTTokenReaderMultipart(JSContext* cx,
                                                       ErrorReporter* er,
                                                       const uint8_t* start,
                                                       const size_t length)
    : BinASTTokenReaderBase(cx, er, start, length),
      metadata_(nullptr),
      posBeforeTree_(nullptr) {
  MOZ_ASSERT(er);
}

BinASTTokenReaderMultipart::~BinASTTokenReaderMultipart() {
  if (metadata_ && metadataOwned_ == MetadataOwnership::Owned) {
    UniqueBinASTSourceMetadataPtr ptr(metadata_);
  }
}

BinASTSourceMetadata* BinASTTokenReaderMultipart::takeMetadata() {
  MOZ_ASSERT(metadataOwned_ == MetadataOwnership::Owned);
  metadataOwned_ = MetadataOwnership::Unowned;
  return metadata_;
}

JS::Result<Ok> BinASTTokenReaderMultipart::initFromScriptSource(
    ScriptSource* scriptSource) {
  metadata_ = scriptSource->binASTSourceMetadata();
  metadataOwned_ = MetadataOwnership::Unowned;

  return Ok();
}

JS::Result<Ok> BinASTTokenReaderMultipart::readHeader() {
  // Check that we don't call this function twice.
  MOZ_ASSERT(!posBeforeTree_);

  // Read global headers.
  MOZ_TRY(readConst(MAGIC_HEADER));
  BINJS_MOZ_TRY_DECL(version, readInternalUint32());

  // For the moment, MAGIC_FORMAT_VERSION is 0. Once we have a story
  // on backwards compatibility of the binary container, we will
  // probably want to change this to `if (version > MAGIC_FORMAT_VERSION)`.
  if (version != MAGIC_FORMAT_VERSION) {
    return raiseError("Format version not implemented");
  }

  // Start reading grammar.
  MOZ_TRY(readConst(SECTION_HEADER_GRAMMAR));
  MOZ_TRY(readConst(COMPRESSION_IDENTITY));  // For the moment, we only support
                                             // identity compression.
  BINJS_MOZ_TRY_DECL(grammarByteLen, readInternalUint32());
  const auto posBeforeGrammar = current_;

  if (posBeforeGrammar + grammarByteLen > stop_ ||
      posBeforeGrammar + grammarByteLen < current_) {  // Sanity check.
    return raiseError("Invalid byte length in grammar table");
  }

  BINJS_MOZ_TRY_DECL(grammarNumberOfEntries, readInternalUint32());
  if (grammarNumberOfEntries > BINASTKIND_LIMIT) {  // Sanity check.
    return raiseError("Invalid number of entries in grammar table");
  }

  // This table maps BinASTKind index -> BinASTKind.
  // Initialize and populate.
  Vector<BinASTKind> grammarTable_(cx_);
  if (!grammarTable_.reserve(grammarNumberOfEntries)) {
    return raiseOOM();
  }

  for (uint32_t i = 0; i < grammarNumberOfEntries; ++i) {
    BINJS_MOZ_TRY_DECL(byteLen, readInternalUint32());
    if (current_ + byteLen > stop_) {
      return raiseError("Invalid byte length in grammar table");
    }
    if (current_ + byteLen < current_) {  // Overflow.
      return raiseError("Invalid byte length in grammar table");
    }
    CharSlice name((const char*)current_, byteLen);
    current_ += byteLen;

    BINJS_MOZ_TRY_DECL(kind, cx_->runtime()->binast().binASTKind(cx_, name));
    if (!kind) {
      return raiseError("Invalid entry in grammar table");
    }

    grammarTable_.infallibleAppend(
        *kind);  // We called `reserve` before the loop.
  }
  if (current_ != grammarByteLen + posBeforeGrammar) {
    return raiseError(
        "The length of the grammar table didn't match its contents.");
  }

  // Start reading strings
  MOZ_TRY(readConst(SECTION_HEADER_STRINGS));
  MOZ_TRY(readConst(COMPRESSION_IDENTITY));  // For the moment, we only support
                                             // identity compression.
  BINJS_MOZ_TRY_DECL(stringsByteLen, readInternalUint32());
  const auto posBeforeStrings = current_;

  if (posBeforeStrings + stringsByteLen > stop_ ||
      posBeforeStrings + stringsByteLen < current_) {  // Sanity check.
    return raiseError("Invalid byte length in strings table");
  }

  BINJS_MOZ_TRY_DECL(stringsNumberOfEntries, readInternalUint32());
  if (stringsNumberOfEntries > MAX_NUMBER_OF_STRINGS) {  // Sanity check.
    return raiseError("Too many entries in strings table");
  }

  BinASTSourceMetadata* metadata =
      BinASTSourceMetadata::Create(grammarTable_, stringsNumberOfEntries);
  if (!metadata) {
    return raiseOOM();
  }

  // Free it if we don't make it out of here alive. Since we don't want to
  // calloc(), we need to avoid marking atoms that might not be there.
  auto se = mozilla::MakeScopeExit([metadata]() { js_free(metadata); });

  RootedAtom atom(cx_);
  for (uint32_t i = 0; i < stringsNumberOfEntries; ++i) {
    BINJS_MOZ_TRY_DECL(byteLen, readInternalUint32());
    if (current_ + byteLen > stop_ || current_ + byteLen < current_) {
      return raiseError("Invalid byte length in individual string");
    }

    // Check null string.
    if (byteLen == 2 && *current_ == 255 && *(current_ + 1) == 0) {
      atom = nullptr;
    } else {
      BINJS_TRY_VAR(atom,
                    AtomizeWTF8Chars(cx_, (const char*)current_, byteLen));
    }

    metadata->getAtom(i) = atom;

    // Populate `slicesTable_`: i => slice
    new (&metadata->getSlice(i)) Chars((const char*)current_, byteLen);

    current_ += byteLen;
  }

  if (posBeforeStrings + stringsByteLen != current_) {
    return raiseError(
        "The length of the strings table didn't match its contents.");
  }

  MOZ_ASSERT(!metadata_);
  se.release();
  metadata_ = metadata;
  metadataOwned_ = MetadataOwnership::Owned;

  // Start reading AST.
  MOZ_TRY(readConst(SECTION_HEADER_TREE));
  MOZ_TRY(readConst(COMPRESSION_IDENTITY));  // For the moment, we only support
                                             // identity compression.
  posBeforeTree_ = current_;

  BINJS_MOZ_TRY_DECL(treeByteLen, readInternalUint32());

  if (posBeforeTree_ + treeByteLen > stop_ ||
      posBeforeTree_ + treeByteLen < posBeforeTree_) {  // Sanity check.
    return raiseError("Invalid byte length in tree table");
  }

  // At this stage, we're ready to start reading the tree.
  return Ok();
}

void BinASTTokenReaderMultipart::traceMetadata(JSTracer* trc) {
  if (metadata_) {
    metadata_->trace(trc);
  }
}

JS::Result<bool> BinASTTokenReaderMultipart::readBool(const Context&) {
  updateLatestKnownGood();
  BINJS_MOZ_TRY_DECL(byte, readByte());

  switch (byte) {
    case 0:
      return false;
    case 1:
      return true;
    case 2:
      return raiseError("Not implemented: null boolean value");
    default:
      return raiseError("Invalid boolean value");
  }
}

// Nullable doubles (little-endian)
//
// NULL_FLOAT_REPRESENTATION (signaling NaN) => null
// anything other 64 bit sequence => IEEE-764 64-bit floating point number
JS::Result<double> BinASTTokenReaderMultipart::readDouble(const Context&) {
  updateLatestKnownGood();

  uint8_t bytes[8];
  MOZ_ASSERT(sizeof(bytes) == sizeof(double));
  MOZ_TRY(
      readBuf(reinterpret_cast<uint8_t*>(bytes), mozilla::ArrayLength(bytes)));

  // Decode little-endian.
  const uint64_t asInt = mozilla::LittleEndian::readUint64(bytes);

  if (asInt == NULL_FLOAT_REPRESENTATION) {
    return raiseError("Not implemented: null double value");
  }

  // Canonicalize NaN, just to make sure another form of signalling NaN
  // doesn't slip past us.
  return JS::CanonicalizeNaN(mozilla::BitwiseCast<double>(asInt));
}

// A single atom is represented as an index into the table of strings.
JS::Result<JSAtom*> BinASTTokenReaderMultipart::readMaybeAtom(const Context&) {
  updateLatestKnownGood();
  BINJS_MOZ_TRY_DECL(index, readInternalUint32());

  if (index >= metadata_->numStrings()) {
    return raiseError("Invalid index to strings table");
  }
  return metadata_->getAtom(index);
}

JS::Result<JSAtom*> BinASTTokenReaderMultipart::readAtom(
    const Context& context) {
  BINJS_MOZ_TRY_DECL(maybe, readMaybeAtom(context));

  if (!maybe) {
    return raiseError("Empty string");
  }

  return maybe;
}

JS::Result<JSAtom*> BinASTTokenReaderMultipart::readMaybeIdentifierName(
    const Context& context) {
  BINJS_MOZ_TRY_DECL(result, readMaybeAtom(context));
  if (result) {
    if (!IsIdentifier(result)) {
      return raiseError("Invalid identifier");
    }
  }
  return result;
}

JS::Result<JSAtom*> BinASTTokenReaderMultipart::readIdentifierName(
    const Context& context) {
  BINJS_MOZ_TRY_DECL(result, readAtom(context));
  if (!IsIdentifier(result)) {
    return raiseError("Invalid identifier");
  }
  return result;
}

JS::Result<JSAtom*> BinASTTokenReaderMultipart::readPropertyKey(
    const Context& context) {
  return readAtom(context);
}

JS::Result<Ok> BinASTTokenReaderMultipart::readChars(Chars& out,
                                                     const Context&) {
  updateLatestKnownGood();
  BINJS_MOZ_TRY_DECL(index, readInternalUint32());

  if (index >= metadata_->numStrings()) {
    return raiseError("Invalid index to strings table for string enum");
  }

  out = metadata_->getSlice(index);
  return Ok();
}

JS::Result<BinASTVariant> BinASTTokenReaderMultipart::readVariant(
    const Context&) {
  updateLatestKnownGood();
  BINJS_MOZ_TRY_DECL(index, readInternalUint32());

  if (index >= metadata_->numStrings()) {
    return raiseError("Invalid index to strings table for string enum");
  }

  auto variantsPtr = variantsTable_.lookupForAdd(index);
  if (variantsPtr) {
    return variantsPtr->value();
  }

  // Either we haven't cached the result yet or this is not a variant.
  // Check in the slices table and, in case of success, cache the result.

  // Note that we stop parsing if we attempt to readVariant() with an
  // ill-formed variant, so we don't run the risk of feching an ill-variant
  // more than once.
  Chars slice = metadata_->getSlice(index);  // We have checked `index` above.
  BINJS_MOZ_TRY_DECL(variant,
                     cx_->runtime()->binast().binASTVariant(cx_, slice));

  if (!variant) {
    return raiseError("Invalid string enum variant");
  }

  if (!variantsTable_.add(variantsPtr, index, *variant)) {
    return raiseOOM();
  }

  return *variant;
}

JS::Result<BinASTTokenReaderBase::SkippableSubTree>
BinASTTokenReaderMultipart::readSkippableSubTree(const Context&) {
  updateLatestKnownGood();
  BINJS_MOZ_TRY_DECL(byteLen, readInternalUint32());

  if (current_ + byteLen > stop_ || current_ + byteLen < current_) {
    return raiseError("Invalid byte length in readSkippableSubTree");
  }

  const auto start = offset();

  current_ += byteLen;

  return BinASTTokenReaderBase::SkippableSubTree(start, byteLen);
}

// Tagged tuples:
// - uint32_t index in table [grammar];
// - content (specified by the higher-level grammar);
JS::Result<Ok> BinASTTokenReaderMultipart::enterTaggedTuple(
    BinASTKind& tag, BinASTTokenReaderMultipart::BinASTFields&, const Context&,
    AutoTaggedTuple& guard) {
  BINJS_MOZ_TRY_DECL(index, readInternalUint32());
  if (index >= metadata_->numBinASTKinds()) {
    return raiseError("Invalid index to grammar table");
  }

  tag = metadata_->getBinASTKind(index);

  // Enter the body.
  guard.init();
  return Ok();
}

// List:
//
// - uint32_t number of items;
// - contents (specified by higher-level grammar);
//
// The total byte length of `number of items` + `contents` must be `byte
// length`.
JS::Result<Ok> BinASTTokenReaderMultipart::enterList(uint32_t& items,
                                                     const Context&,
                                                     AutoList& guard) {
  guard.init();

  MOZ_TRY_VAR(items, readInternalUint32());

  return Ok();
}

void BinASTTokenReaderMultipart::AutoBase::init() { initialized_ = true; }

BinASTTokenReaderMultipart::AutoBase::AutoBase(
    BinASTTokenReaderMultipart& reader)
    : initialized_(false), reader_(reader) {}

BinASTTokenReaderMultipart::AutoBase::~AutoBase() {
  // By now, the `AutoBase` must have been deinitialized by calling `done()`.
  // The only case in which we can accept not calling `done()` is if we have
  // bailed out because of an error.
  MOZ_ASSERT_IF(initialized_, reader_.hasRaisedError());
}

JS::Result<Ok> BinASTTokenReaderMultipart::AutoBase::checkPosition(
    const uint8_t* expectedEnd) {
  if (reader_.current_ != expectedEnd) {
    return reader_.raiseError(
        "Caller did not consume the expected set of bytes");
  }

  return Ok();
}

BinASTTokenReaderMultipart::AutoList::AutoList(
    BinASTTokenReaderMultipart& reader)
    : AutoBase(reader) {}

void BinASTTokenReaderMultipart::AutoList::init() { AutoBase::init(); }

JS::Result<Ok> BinASTTokenReaderMultipart::AutoList::done() {
  MOZ_ASSERT(initialized_);
  initialized_ = false;
  if (reader_.hasRaisedError()) {
    // Already errored, no need to check further.
    return reader_.cx_->alreadyReportedError();
  }

  return Ok();
}

// Internal uint32_t
//
// Encoded as variable length number.

MOZ_MUST_USE JS::Result<uint32_t>
BinASTTokenReaderMultipart::readInternalUint32() {
  uint32_t result = 0;
  uint32_t shift = 0;
  while (true) {
    MOZ_ASSERT(shift < 32);
    uint32_t byte;
    MOZ_TRY_VAR(byte, readByte());

    const uint32_t newResult = result | (byte >> 1) << shift;
    if (newResult < result) {
      return raiseError("Overflow during readInternalUint32");
    }

    result = newResult;
    shift += 7;

    if ((byte & 1) == 0) {
      return result;
    }

    if (shift >= 32) {
      return raiseError("Overflow during readInternalUint32");
    }
  }
}

BinASTTokenReaderMultipart::AutoTaggedTuple::AutoTaggedTuple(
    BinASTTokenReaderMultipart& reader)
    : AutoBase(reader) {}

JS::Result<Ok> BinASTTokenReaderMultipart::AutoTaggedTuple::done() {
  MOZ_ASSERT(initialized_);
  initialized_ = false;
  if (reader_.hasRaisedError()) {
    // Already errored, no need to check further.
    return reader_.cx_->alreadyReportedError();
  }

  return Ok();
}

}  // namespace frontend

}  // namespace js
