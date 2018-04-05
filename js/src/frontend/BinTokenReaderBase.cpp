/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BinTokenReaderBase.h"

#include "frontend/BinSource-macros.h"
#include "gc/Zone.h"

namespace js {
namespace frontend {

template<typename T> using ErrorResult = mozilla::GenericErrorResult<T>;

// We use signalling NaN (which doesn't exist in the JS syntax)
// to represent a `null` number.
const uint64_t NULL_FLOAT_REPRESENTATION = 0x7FF0000000000001;

void
BinTokenReaderBase::updateLatestKnownGood()
{
    MOZ_ASSERT(current_ >= start_);
    const size_t update = current_ - start_;
    MOZ_ASSERT(update >= latestKnownGoodPos_);
    latestKnownGoodPos_ = update;
}

ErrorResult<JS::Error&>
BinTokenReaderBase::raiseError(const char* description)
{
    MOZ_ASSERT(!cx_->isExceptionPending());
    TokenPos pos = this->pos();
    JS_ReportErrorASCII(cx_, "BinAST parsing error: %s at offsets %u => %u",
                        description, pos.begin, pos.end);
    return cx_->alreadyReportedError();
}

ErrorResult<JS::Error&>
BinTokenReaderBase::raiseOOM()
{
    ReportOutOfMemory(cx_);
    return cx_->alreadyReportedError();
}

ErrorResult<JS::Error&>
BinTokenReaderBase::raiseInvalidNumberOfFields(const BinKind kind, const uint32_t expected, const uint32_t got)
{
    Sprinter out(cx_);
    BINJS_TRY(out.init());
    BINJS_TRY(out.printf("In %s, invalid number of fields: expected %u, got %u",
        describeBinKind(kind), expected, got));
    return raiseError(out.string());
}

ErrorResult<JS::Error&>
BinTokenReaderBase::raiseInvalidField(const char* kind, const BinField field)
{
    Sprinter out(cx_);
    BINJS_TRY(out.init());
    BINJS_TRY(out.printf("In %s, invalid field '%s'", kind, describeBinField(field)));
    return raiseError(out.string());
}


size_t
BinTokenReaderBase::offset() const
{
    return current_ - start_;
}

TokenPos
BinTokenReaderBase::pos()
{
    return pos(offset());
}

TokenPos
BinTokenReaderBase::pos(size_t start)
{
    TokenPos pos;
    pos.begin = start;
    pos.end = current_ - start_;
    MOZ_ASSERT(pos.end >= pos.begin);
    return pos;
}

JS::Result<Ok>
BinTokenReaderBase::readBuf(uint8_t* bytes, uint32_t len)
{
    MOZ_ASSERT(!cx_->isExceptionPending());
    MOZ_ASSERT(len > 0);

    if (stop_ < current_ + len)
        return raiseError("Buffer exceeds length");

    for (uint32_t i = 0; i < len; ++i)
        *bytes++ = *current_++;

    return Ok();
}

JS::Result<uint8_t>
BinTokenReaderBase::readByte()
{
    uint8_t byte;
    MOZ_TRY(readBuf(&byte, 1));
    return byte;
}

}
}
