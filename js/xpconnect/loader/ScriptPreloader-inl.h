/* -*-  Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ScriptPreloader_inl_h
#define ScriptPreloader_inl_h

#include "mozilla/Attributes.h"
#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/EnumSet.h"
#include "mozilla/Range.h"
#include "mozilla/Result.h"
#include "mozilla/Unused.h"
#include "nsString.h"
#include "nsTArray.h"

#include <prio.h>

namespace mozilla {
namespace loader {

static inline Result<Ok, nsresult>
WrapNSResult(PRStatus aRv)
{
    if (aRv != PR_SUCCESS) {
        return Err(NS_ERROR_FAILURE);
    }
    return Ok();
}

static inline Result<Ok, nsresult>
WrapNSResult(nsresult aRv)
{
    if (NS_FAILED(aRv)) {
        return Err(aRv);
    }
    return Ok();
}

#define NS_TRY(expr) MOZ_TRY(WrapNSResult(expr))


class OutputBuffer
{
public:
    OutputBuffer()
    {}

    uint8_t*
    write(size_t size)
    {
        auto buf = data.AppendElements(size);
        cursor_ += size;
        return buf;
    }

    void
    codeUint8(const uint8_t& val)
    {
        *write(sizeof val) = val;
    }

    template<typename T>
    void
    codeUint8(const EnumSet<T>& val)
    {
        // EnumSets are always represented as uint32_t values, so we need to
        // assert that the value actually fits in a uint8 before writing it.
        uint32_t value = val.serialize();
        codeUint8(CheckedUint8(value).value());
    }

    void
    codeUint16(const uint16_t& val)
    {
        LittleEndian::writeUint16(write(sizeof val), val);
    }

    void
    codeUint32(const uint32_t& val)
    {
        LittleEndian::writeUint32(write(sizeof val), val);
    }

    void
    codeString(const nsCString& str)
    {
        auto len = CheckedUint16(str.Length()).value();

        codeUint16(len);
        memcpy(write(len), str.BeginReading(), len);
    }

    size_t cursor() const { return cursor_; }


    uint8_t* Get() { return data.Elements(); }

    const uint8_t* Get() const { return data.Elements(); }

private:
    nsTArray<uint8_t> data;
    size_t cursor_ = 0;
};

class InputBuffer
{
public:
    explicit InputBuffer(const Range<uint8_t>& buffer)
        : data(buffer)
    {}

    const uint8_t*
    read(size_t size)
    {
        MOZ_ASSERT(checkCapacity(size));

        auto buf = &data[cursor_];
        cursor_ += size;
        return buf;
    }

    bool
    codeUint8(uint8_t& val)
    {
        if (checkCapacity(sizeof val)) {
            val = *read(sizeof val);
        }
        return !error_;
    }

    template<typename T>
    bool
    codeUint8(EnumSet<T>& val)
    {
        uint8_t value;
        if (codeUint8(value)) {
            val.deserialize(value);
        }
        return !error_;
    }

    bool
    codeUint16(uint16_t& val)
    {
        if (checkCapacity(sizeof val)) {
            val = LittleEndian::readUint16(read(sizeof val));
        }
        return !error_;
    }

    bool
    codeUint32(uint32_t& val)
    {
        if (checkCapacity(sizeof val)) {
            val = LittleEndian::readUint32(read(sizeof val));
        }
        return !error_;
    }

    bool
    codeString(nsCString& str)
    {
        uint16_t len;
        if (codeUint16(len)) {
            if (checkCapacity(len)) {
                str.SetLength(len);
                memcpy(str.BeginWriting(), read(len), len);
            }
        }
        return !error_;
    }

    bool error() { return error_; }

    bool finished() { return error_ || !remainingCapacity(); }

    size_t remainingCapacity() { return data.length() - cursor_; }

    size_t cursor() const { return cursor_; }

    const uint8_t* Get() const { return data.begin().get(); }

private:
    bool
    checkCapacity(size_t size)
    {
        if (size > remainingCapacity()) {
            error_ = true;
        }
        return !error_;
    }

    bool error_ = false;

public:
    const Range<uint8_t>& data;
    size_t cursor_ = 0;
};

}; // namespace loader
}; // namespace mozilla

#endif // ScriptPreloader_inl_h
