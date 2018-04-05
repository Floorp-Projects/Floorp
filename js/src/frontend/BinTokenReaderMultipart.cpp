/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BinTokenReaderMultipart.h"

#include "mozilla/EndianUtils.h"
#include "mozilla/Maybe.h"

#include "frontend/BinSource-macros.h"
#include "frontend/BinSourceRuntimeSupport.h"

#include "gc/Zone.h"

namespace js {
namespace frontend {

// The magic header, at the start of every binjs file.
const char MAGIC_HEADER[] = "BINJS";
// The latest format version understood by this tokenizer.
const uint32_t MAGIC_FORMAT_VERSION = 0;

// The headers at the start of each section of the binjs file.
const char SECTION_HEADER_GRAMMAR[] = "[GRAMMAR]";
const char SECTION_HEADER_STRINGS[] = "[STRINGS]";
const char SECTION_HEADER_TREE[] = "[TREE]";

// The (only) internal compression mechanism understood by this parser.
const char COMPRESSION_IDENTITY[] = "identity;";


// The maximal number of distinct strings that may be declared in a
// single file.
const uint32_t MAX_NUMBER_OF_STRINGS = 32768;

using AutoList = BinTokenReaderMultipart::AutoList;
using AutoTaggedTuple = BinTokenReaderMultipart::AutoTaggedTuple;
using AutoTuple = BinTokenReaderMultipart::AutoTuple;
using CharSlice = BinaryASTSupport::CharSlice;
using Chars = BinTokenReaderMultipart::Chars;

BinTokenReaderMultipart::BinTokenReaderMultipart(JSContext* cx, const uint8_t* start, const size_t length)
  : BinTokenReaderBase(cx, start, length)
  , grammarTable_(cx)
  , atomsTable_(cx, AtomVector(cx))
  , slicesTable_(cx)
  , posBeforeTree_(nullptr)
{ }

JS::Result<Ok>
BinTokenReaderMultipart::readHeader()
{
    // Check that we don't call this function twice.
    MOZ_ASSERT(!posBeforeTree_);

    // Read global headers.
    MOZ_TRY(readConst(MAGIC_HEADER));
    BINJS_MOZ_TRY_DECL(version, readInternalUint32());

    // For the moment, MAGIC_FORMAT_VERSION is 0. Once we have a story
    // on backwards compatibility of the binary container, we will
    // probably want to change this to `if (version > MAGIC_FORMAT_VERSION)`.
    if (version != MAGIC_FORMAT_VERSION)
        return raiseError("Format version not implemented");

    // Start reading grammar.
    MOZ_TRY(readConst(SECTION_HEADER_GRAMMAR));
    MOZ_TRY(readConst(COMPRESSION_IDENTITY)); // For the moment, we only support identity compression.
    BINJS_MOZ_TRY_DECL(grammarByteLen, readInternalUint32());
    const auto posBeforeGrammar = current_;

    if (posBeforeGrammar + grammarByteLen > stop_ || posBeforeGrammar + grammarByteLen < current_) // Sanity check.
        return raiseError("Invalid byte length in grammar table");

    BINJS_MOZ_TRY_DECL(grammarNumberOfEntries, readInternalUint32());
    if (grammarNumberOfEntries > BINKIND_LIMIT) // Sanity check.
        return raiseError("Invalid number of entries in grammar table");

    // This table maps BinKind index -> BinKind.
    // Initialize and populate.
    if (!grammarTable_.reserve(grammarNumberOfEntries))
        return raiseOOM();

    for (uint32_t i = 0; i < grammarNumberOfEntries; ++i) {
        BINJS_MOZ_TRY_DECL(byteLen, readInternalUint32());
        if (current_ + byteLen > stop_)
            return raiseError("Invalid byte length in grammar table");
        if (current_ + byteLen < current_) // Overflow.
            return raiseError("Invalid byte length in grammar table");
        CharSlice name((const char*)current_, byteLen);
        current_ += byteLen;

        BINJS_MOZ_TRY_DECL(kind, cx_->runtime()->binast().binKind(cx_, name));
        if (!kind)
            return raiseError("Invalid entry in grammar table");

        grammarTable_.infallibleAppend(*kind); // We called `reserve` before the loop.
    }
    if (current_ != grammarByteLen + posBeforeGrammar)
        return raiseError("The length of the grammar table didn't match its contents.");

    // Start reading strings
    MOZ_TRY(readConst(SECTION_HEADER_STRINGS));
    MOZ_TRY(readConst(COMPRESSION_IDENTITY)); // For the moment, we only support identity compression.
    BINJS_MOZ_TRY_DECL(stringsByteLen, readInternalUint32());
    const auto posBeforeStrings = current_;

    if (posBeforeStrings + stringsByteLen > stop_ || posBeforeStrings + stringsByteLen < current_) // Sanity check.
        return raiseError("Invalid byte length in strings table");

    BINJS_MOZ_TRY_DECL(stringsNumberOfEntries, readInternalUint32());
    if (stringsNumberOfEntries > MAX_NUMBER_OF_STRINGS) // Sanity check.
        return raiseError("Too many entries in strings table");

    // This table maps String index -> String.
    // Initialize and populate.
    if (!atomsTable_.reserve(stringsNumberOfEntries))
        return raiseOOM();
    if (!slicesTable_.reserve(stringsNumberOfEntries))
        return raiseOOM();
    if (!variantsTable_.init())
        return cx_->alreadyReportedError();

    RootedAtom atom(cx_);
    for (uint32_t i = 0; i < stringsNumberOfEntries; ++i) {
        BINJS_MOZ_TRY_DECL(byteLen, readInternalUint32());
        if (current_ + byteLen > stop_ || current_ + byteLen < current_)
            return raiseError("Invalid byte length in individual string");

        // Check null string.
        if (byteLen == 2 && *current_ == 255 && *(current_ + 1) == 0) {
            atom = nullptr;
        } else {
            BINJS_TRY_VAR(atom, Atomize(cx_, (const char*)current_, byteLen));
        }

        // Populate `atomsTable_`: i => atom.
        atomsTable_.infallibleAppend(atom); // We have reserved before entering the loop.

        // Populate `slicesTable_`: i => slice
        Chars slice((const char*)current_, byteLen);
        slicesTable_.infallibleAppend(Move(slice)); // We have reserved before entering the loop.

        current_ += byteLen;
    }

    if (posBeforeStrings + stringsByteLen != current_)
        return raiseError("The length of the strings table didn't match its contents.");

    // Start reading AST.
    MOZ_TRY(readConst(SECTION_HEADER_TREE));
    MOZ_TRY(readConst(COMPRESSION_IDENTITY)); // For the moment, we only support identity compression.
    posBeforeTree_ = current_;

    BINJS_MOZ_TRY_DECL(treeByteLen, readInternalUint32());

    if (posBeforeTree_ + treeByteLen > stop_ || posBeforeTree_ + treeByteLen < posBeforeTree_) // Sanity check.
        return raiseError("Invalid byte length in tree table");

    // At this stage, we're ready to start reading the tree.
    return Ok();
}

JS::Result<bool>
BinTokenReaderMultipart::readBool()
{
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
JS::Result<double>
BinTokenReaderMultipart::readDouble()
{
    updateLatestKnownGood();

    uint8_t bytes[8];
    MOZ_ASSERT(sizeof(bytes) == sizeof(double));
    MOZ_TRY(readBuf(reinterpret_cast<uint8_t*>(bytes), ArrayLength(bytes)));

    // Decode little-endian.
    const uint64_t asInt = LittleEndian::readUint64(bytes);

    if (asInt == NULL_FLOAT_REPRESENTATION)
        return raiseError("Not implemented: null double value");

    // Canonicalize NaN, just to make sure another form of signalling NaN
    // doesn't slip past us.
    const double asDouble = CanonicalizeNaN(BitwiseCast<double>(asInt));
    return asDouble;
}


// A single atom is represented as an index into the table of strings.
JS::Result<JSAtom*>
BinTokenReaderMultipart::readMaybeAtom()
{
    updateLatestKnownGood();
    BINJS_MOZ_TRY_DECL(index, readInternalUint32());

    if (index >= atomsTable_.length())
        return raiseError("Invalid index to strings table");
    return atomsTable_[index].get();
}

JS::Result<JSAtom*>
BinTokenReaderMultipart::readAtom()
{
    BINJS_MOZ_TRY_DECL(maybe, readMaybeAtom());

    if (!maybe)
        return raiseError("Empty string");

    return maybe;
}

JS::Result<Ok>
BinTokenReaderMultipart::readChars(Chars& out)
{
    updateLatestKnownGood();
    BINJS_MOZ_TRY_DECL(index, readInternalUint32());

    if (index >= slicesTable_.length())
        return raiseError("Invalid index to strings table for string enum");

    out = slicesTable_[index];
    return Ok();
}

JS::Result<BinVariant>
BinTokenReaderMultipart::readVariant()
{
    updateLatestKnownGood();
    BINJS_MOZ_TRY_DECL(index, readInternalUint32());

    if (index >= slicesTable_.length())
        return raiseError("Invalid index to strings table for string enum");

    auto variantsPtr = variantsTable_.lookupForAdd(index);
    if (variantsPtr)
        return variantsPtr->value();

    // Either we haven't cached the result yet or this is not a variant.
    // Check in the slices table and, in case of success, cache the result.

    // Note that we stop parsing if we attempt to readVariant() with an
    // ill-formed variant, so we don't run the risk of feching an ill-variant
    // more than once.
    Chars slice = slicesTable_[index]; // We have checked `index` above.
    BINJS_MOZ_TRY_DECL(variant, cx_->runtime()->binast().binVariant(cx_, slice));

    if (!variant)
        return raiseError("Invalid string enum variant");

    if (!variantsTable_.add(variantsPtr, index, *variant))
        return raiseOOM();

    return *variant;
}

// Untagged tuple:
// - contents (specified by the higher-level grammar);
JS::Result<Ok>
BinTokenReaderMultipart::enterUntaggedTuple(AutoTuple& guard)
{
    guard.init();
    return Ok();
}


// Tagged tuples:
// - uint32_t index in table [grammar];
// - content (specified by the higher-level grammar);
JS::Result<Ok>
BinTokenReaderMultipart::enterTaggedTuple(BinKind& tag, BinTokenReaderMultipart::BinFields&, AutoTaggedTuple& guard)
{
    BINJS_MOZ_TRY_DECL(index, readInternalUint32());
    if (index >= grammarTable_.length())
        return raiseError("Invalid index to grammar table");

    tag = grammarTable_[index];

    // Enter the body.
    guard.init();
    return Ok();
}

// List:
//
// - uint32_t number of items;
// - contents (specified by higher-level grammar);
//
// The total byte length of `number of items` + `contents` must be `byte length`.
JS::Result<Ok>
BinTokenReaderMultipart::enterList(uint32_t& items, AutoList& guard)
{
    guard.init();

    MOZ_TRY_VAR(items, readInternalUint32());

    return Ok();
}

void
BinTokenReaderMultipart::AutoBase::init()
{
    initialized_ = true;
}

BinTokenReaderMultipart::AutoBase::AutoBase(BinTokenReaderMultipart& reader)
    : reader_(reader)
{ }

BinTokenReaderMultipart::AutoBase::~AutoBase()
{
    // By now, the `AutoBase` must have been deinitialized by calling `done()`.
    // The only case in which we can accept not calling `done()` is if we have
    // bailed out because of an error.
    MOZ_ASSERT_IF(initialized_, reader_.cx_->isExceptionPending());
}

JS::Result<Ok>
BinTokenReaderMultipart::AutoBase::checkPosition(const uint8_t* expectedEnd)
{
    if (reader_.current_ != expectedEnd)
        return reader_.raiseError("Caller did not consume the expected set of bytes");

    return Ok();
}



BinTokenReaderMultipart::AutoList::AutoList(BinTokenReaderMultipart& reader)
    : AutoBase(reader)
{ }

void
BinTokenReaderMultipart::AutoList::init()
{
    AutoBase::init();
}

JS::Result<Ok>
BinTokenReaderMultipart::AutoList::done()
{
    MOZ_ASSERT(initialized_);
    initialized_ = false;
    if (reader_.cx_->isExceptionPending()) {
        // Already errored, no need to check further.
        return reader_.cx_->alreadyReportedError();
    }

    return Ok();
}


// Internal uint32_t
//
// Encoded as variable length number.

MOZ_MUST_USE JS::Result<uint32_t>
BinTokenReaderMultipart::readInternalUint32()
{
    uint32_t result = 0;
    uint32_t shift  = 0;
    while (true) {
        MOZ_ASSERT(shift < 32);
        uint32_t byte;
        MOZ_TRY_VAR(byte, readByte());

        const uint32_t newResult = result | (byte >> 1) << shift;
        if (newResult < result)
            return raiseError("Overflow during readInternalUint32");

        result = newResult;
        shift += 7;

        if ((byte & 1) == 0) {
            return result;
        }
    }
}


BinTokenReaderMultipart::AutoTaggedTuple::AutoTaggedTuple(BinTokenReaderMultipart& reader)
    : AutoBase(reader)
{ }

JS::Result<Ok>
BinTokenReaderMultipart::AutoTaggedTuple::done()
{
    MOZ_ASSERT(initialized_);
    initialized_ = false;
    if (reader_.cx_->isExceptionPending()) {
        // Already errored, no need to check further.
        return reader_.cx_->alreadyReportedError();
    }

    return Ok();
}

BinTokenReaderMultipart::AutoTuple::AutoTuple(BinTokenReaderMultipart& reader)
    : AutoBase(reader)
{ }

JS::Result<Ok>
BinTokenReaderMultipart::AutoTuple::done()
{
    MOZ_ASSERT(initialized_);
    initialized_ = false;
    if (reader_.cx_->isExceptionPending()) {
        // Already errored, no need to check further.
        return reader_.cx_->alreadyReportedError();
    }

    // Check suffix.
    return Ok();
}

} // namespace frontend


} // namespace js

