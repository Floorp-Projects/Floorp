/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "frontend/BinTokenReaderTester.h"

#include "mozilla/EndianUtils.h"

#include "frontend/BinSource-macros.h"
#include "gc/Zone.h"

namespace js {
namespace frontend {

using BinFields = BinTokenReaderTester::BinFields;
using AutoList = BinTokenReaderTester::AutoList;
using AutoTaggedTuple = BinTokenReaderTester::AutoTaggedTuple;
using AutoTuple = BinTokenReaderTester::AutoTuple;

BinTokenReaderTester::BinTokenReaderTester(JSContext* cx, const uint8_t* start, const size_t length)
    : BinTokenReaderBase(cx, start, length)
{ }

BinTokenReaderTester::BinTokenReaderTester(JSContext* cx, const Vector<uint8_t>& buf)
    : BinTokenReaderBase(cx, buf.begin(), buf.length())
{ }

JS::Result<Ok>
BinTokenReaderTester::readHeader()
{
    // This format does not have a header.
    return Ok();
}

JS::Result<bool>
BinTokenReaderTester::readBool()
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
BinTokenReaderTester::readDouble()
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

// Internal uint32_t
//
// Encoded as 4 bytes, little-endian.
MOZ_MUST_USE JS::Result<uint32_t>
BinTokenReaderTester::readInternalUint32()
{
    uint8_t bytes[4];
    MOZ_ASSERT(sizeof(bytes) == sizeof(uint32_t));
    MOZ_TRY(readBuf(bytes, 4));

    // Decode little-endian.
    return LittleEndian::readUint32(bytes);
}



// Nullable strings:
// - "<string>" (not counted in byte length)
// - byte length (not counted in byte length)
// - bytes (UTF-8)
// - "</string>" (not counted in byte length)
//
// The special sequence of bytes `[255, 0]` (which is an invalid UTF-8 sequence)
// is reserved to `null`.
JS::Result<JSAtom*>
BinTokenReaderTester::readMaybeAtom()
{
    updateLatestKnownGood();

    MOZ_TRY(readConst("<string>"));

    RootedAtom result(cx_);

    // 1. Read byteLength
    BINJS_MOZ_TRY_DECL(byteLen, readInternalUint32());

    // 2. Reject if we can't read
    if (current_ + byteLen < current_) // Check for overflows
        return raiseError("Arithmetics overflow: string is too long");

    if (current_ + byteLen > stop_)
        return raiseError("Not enough bytes to read chars");

    if (byteLen == 2 && *current_ == 255 && *(current_ + 1) == 0) {
        // 3. Special case: null string.
        result = nullptr;
    } else {
        // 4. Other strings (bytes are copied)
        BINJS_TRY_VAR(result, Atomize(cx_, (const char*)current_, byteLen));
    }

    current_ += byteLen;
    MOZ_TRY(readConst("</string>"));
    return result.get();
}


// Nullable strings:
// - "<string>" (not counted in byte length)
// - byte length (not counted in byte length)
// - bytes (UTF-8)
// - "</string>" (not counted in byte length)
//
// The special sequence of bytes `[255, 0]` (which is an invalid UTF-8 sequence)
// is reserved to `null`.
JS::Result<Ok>
BinTokenReaderTester::readChars(Chars& out)
{
    updateLatestKnownGood();

    MOZ_TRY(readConst("<string>"));

    // 1. Read byteLength
    BINJS_MOZ_TRY_DECL(byteLen, readInternalUint32());

    // 2. Reject if we can't read
    if (current_ + byteLen < current_) // Check for overflows
        return raiseError("Arithmetics overflow: string is too long");

    if (current_ + byteLen > stop_)
        return raiseError("Not enough bytes to read chars");

    if (byteLen == 2 && *current_ == 255 && *(current_ + 1) == 0) {
        // 3. Special case: null string.
        return raiseError("Empty string");
    }

    // 4. Other strings (bytes are copied)
    if (!out.resize(byteLen))
        return raiseOOM();

    PodCopy(out.begin(), current_, byteLen);

    current_ += byteLen;

    MOZ_TRY(readConst("</string>"));
    return Ok();
}

JS::Result<JSAtom*>
BinTokenReaderTester::readAtom()
{
    RootedAtom atom(cx_);
    MOZ_TRY_VAR(atom, readMaybeAtom());

    if (!atom)
        return raiseError("Empty string");
    return atom.get();
}

JS::Result<BinVariant>
BinTokenReaderTester::readVariant()
{
    MOZ_TRY(readConst("<string>"));
    BINJS_MOZ_TRY_DECL(byteLen, readInternalUint32());

    // 2. Reject if we can't read
    if (current_ + byteLen < current_) // Check for overflows
        return raiseError("Arithmetics overflow: string is too long");

    if (current_ + byteLen > stop_)
        return raiseError("Not enough bytes to read chars");

    if (byteLen == 2 && *current_ == 255 && *(current_ + 1) == 0) {
        // 3. Special case: null string.
        return raiseError("Empty variant");
    }

    BinaryASTSupport::CharSlice slice((const char*)current_, byteLen);
    current_ += byteLen;

    BINJS_MOZ_TRY_DECL(variant, cx_->runtime()->binast().binVariant(cx_, slice));
    if (!variant)
        return raiseError("Not a variant");

    MOZ_TRY(readConst("</string>"));
    return *variant;
}

JS::Result<BinTokenReaderBase::SkippableSubTree>
BinTokenReaderTester::readSkippableSubTree()
{
    updateLatestKnownGood();
    BINJS_MOZ_TRY_DECL(byteLen, readInternalUint32());

    if (current_ + byteLen > stop_ || current_ + byteLen < current_)
        return raiseError("Invalid byte length in readSkippableSubTree");

    const auto start = current_;

    current_ += byteLen;

    return BinTokenReaderBase::SkippableSubTree(start, byteLen);
}

// Untagged tuple:
// - "<tuple>";
// - contents (specified by the higher-level grammar);
// - "</tuple>"
JS::Result<Ok>
BinTokenReaderTester::enterUntaggedTuple(AutoTuple& guard)
{
    MOZ_TRY(readConst("<tuple>"));

    guard.init();
    return Ok();
}

// Tagged tuples:
// - "<tuple>";
// - "<head>";
// - non-null string `name`, followed by \0 (see `readString()`);
// - uint32_t number of fields;
// - array of `number of fields` non-null strings followed each by \0 (see `readString()`);
// - "</head>";
// - content (specified by the higher-level grammar);
// - "</tuple>"
JS::Result<Ok>
BinTokenReaderTester::enterTaggedTuple(BinKind& tag, BinFields& fields, AutoTaggedTuple& guard)
{
    // Header
    MOZ_TRY(readConst("<tuple>"));
    MOZ_TRY(readConst("<head>"));

    // This would probably be much faster with a HashTable, but we don't
    // really care about the speed of BinTokenReaderTester.
    do {

#define FIND_MATCH(CONSTRUCTOR, NAME) \
        if (matchConst(NAME, true)) { \
            tag = BinKind::CONSTRUCTOR; \
            break; \
        } // else

        FOR_EACH_BIN_KIND(FIND_MATCH)
#undef FIND_MATCH

        // else
        return raiseError("Invalid tag");
    } while(false);

    // Now fields.
    BINJS_MOZ_TRY_DECL(fieldNum, readInternalUint32());

    fields.clear();
    if (!fields.reserve(fieldNum))
        return raiseOOM();

    for (uint32_t i = 0; i < fieldNum; ++i) {
        // This would probably be much faster with a HashTable, but we don't
        // really care about the speed of BinTokenReaderTester.
        BinField field;
        do {

#define FIND_MATCH(CONSTRUCTOR, NAME) \
            if (matchConst(NAME, true)) { \
                field = BinField::CONSTRUCTOR; \
                break; \
            } // else

            FOR_EACH_BIN_FIELD(FIND_MATCH)
#undef FIND_MATCH

            // else
            return raiseError("Invalid field");
        } while (false);

        // Make sure that we do not have duplicate fields.
        // Search is linear, but again, we don't really care
        // in this implementation.
        for (uint32_t j = 0; j < i; ++j) {
            if (fields[j] == field) {
                return raiseError("Duplicate field");
            }
        }

        fields.infallibleAppend(field); // Already checked.
    }

    // End of header
    MOZ_TRY(readConst("</head>"));

    // Enter the body.
    guard.init();
    return Ok();
}

// List:
//
// - "<list>" (not counted in byte length);
// - uint32_t byte length (not counted in byte length);
// - uint32_t number of items;
// - contents (specified by higher-level grammar);
// - "</list>" (not counted in byte length)
//
// The total byte length of `number of items` + `contents` must be `byte length`.
JS::Result<Ok>
BinTokenReaderTester::enterList(uint32_t& items, AutoList& guard)
{
    MOZ_TRY(readConst("<list>"));
    guard.init();

    MOZ_TRY_VAR(items, readInternalUint32());
    return Ok();
}

void
BinTokenReaderTester::AutoBase::init()
{
    initialized_ = true;
}

BinTokenReaderTester::AutoBase::AutoBase(BinTokenReaderTester& reader)
    : reader_(reader)
{ }

BinTokenReaderTester::AutoBase::~AutoBase()
{
    // By now, the `AutoBase` must have been deinitialized by calling `done()`.
    // The only case in which we can accept not calling `done()` is if we have
    // bailed out because of an error.
    MOZ_ASSERT_IF(initialized_, reader_.cx_->isExceptionPending());
}

JS::Result<Ok>
BinTokenReaderTester::AutoBase::checkPosition(const uint8_t* expectedEnd)
{
    if (reader_.current_ != expectedEnd)
        return reader_.raiseError("Caller did not consume the expected set of bytes");

    return Ok();
}

BinTokenReaderTester::AutoList::AutoList(BinTokenReaderTester& reader)
    : AutoBase(reader)
{ }

void
BinTokenReaderTester::AutoList::init()
{
    AutoBase::init();
}

JS::Result<Ok>
BinTokenReaderTester::AutoList::done()
{
    MOZ_ASSERT(initialized_);
    initialized_ = false;
    if (reader_.cx_->isExceptionPending()) {
        // Already errored, no need to check further.
        return reader_.cx_->alreadyReportedError();
    }

    // Check suffix.
    MOZ_TRY(reader_.readConst("</list>"));

    return Ok();
}

BinTokenReaderTester::AutoTaggedTuple::AutoTaggedTuple(BinTokenReaderTester& reader)
    : AutoBase(reader)
{ }

JS::Result<Ok>
BinTokenReaderTester::AutoTaggedTuple::done()
{
    MOZ_ASSERT(initialized_);
    initialized_ = false;
    if (reader_.cx_->isExceptionPending()) {
        // Already errored, no need to check further.
        return reader_.cx_->alreadyReportedError();
    }

    // Check suffix.
    MOZ_TRY(reader_.readConst("</tuple>"));

    return Ok();
}

BinTokenReaderTester::AutoTuple::AutoTuple(BinTokenReaderTester& reader)
    : AutoBase(reader)
{ }

JS::Result<Ok>
BinTokenReaderTester::AutoTuple::done()
{
    MOZ_ASSERT(initialized_);
    initialized_ = false;
    if (reader_.cx_->isExceptionPending()) {
        // Already errored, no need to check further.
        return reader_.cx_->alreadyReportedError();
    }

    // Check suffix.
    MOZ_TRY(reader_.readConst("</tuple>"));

    return Ok();
}

} // namespace frontend
} // namespace js

