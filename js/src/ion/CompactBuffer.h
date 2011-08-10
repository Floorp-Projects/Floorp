/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Anderson <dvander@alliedmods.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef jsion_compact_buffer_h__
#define jsion_compact_buffer_h__

#include "jsvector.h"

namespace js {
namespace ion {

// CompactBuffers are byte streams designed for compressable integers. It has
// helper functions for writing bytes, fixed-size integers, and variable-sized
// integers. Variable sized integers are encoded in 1-5 bytes, each byte
// containing 7 bits of the integer and a bit which specifies whether the next
// byte is also part of the integer.
//
// Fixed-width integers are also available, in case the actual value will not
// be known until later.

class CompactBufferReader
{
    const uint8 *buffer_;
    const uint8 *end_;

    uint32 readVariableLength() {
        uint32 val = 0;
        uint32 shift = 0;
        uint byte;
        while (true) {
            JS_ASSERT(shift < 32);
            byte = readByte();
            val |= (uint32(byte) >> 1) << shift;
            shift += 7;
            if (!(byte & 1))
                return val;
        }
        JS_NOT_REACHED("unreachable");
        return 0;
    }

  public:
    CompactBufferReader(const uint8 *start, const uint8 *end)
      : buffer_(start),
        end_(end)
    { }
    uint8 readByte() {
        JS_ASSERT(buffer_ < end_);
        return *buffer_++;
    }
    uint32 readFixedUint32() {
        uint32 b0 = readByte();
        uint32 b1 = readByte();
        uint32 b2 = readByte();
        uint32 b3 = readByte();
        return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
    }
    uint32 readUnsigned() {
        return readVariableLength();
    }
    int32 readSigned() {
        uint8 b = readByte();
        bool isNegative = !!(b & (1 << 0));
        bool more = !!(b & (1 << 1));
        int32 result = b >> 2;
        if (more)
            result |= readUnsigned() << 6;
        if (isNegative)
            return -result;
        return result;
    }

    bool more() const {
        JS_ASSERT(buffer_ <= end_);
        return buffer_ < end_;
    }
};

class CompactBufferWriter
{
    js::Vector<uint8, 32, SystemAllocPolicy> buffer_;
    bool enoughMemory_;

  public:
    CompactBufferWriter()
      : enoughMemory_(true)
    { }

    // Note: writeByte() takes uint32 to catch implicit casts with a runtime
    // assert.
    void writeByte(uint32 byte) {
        JS_ASSERT(byte <= 0xFF);
        enoughMemory_ &= buffer_.append(byte);
    }
    void writeUnsigned(uint32 value) {
        do {
            uint8 byte = ((value & 0x7F) << 1) | (value > 0x7F);
            writeByte(byte);
            value >>= 7;
        } while (value);
    }
    void writeSigned(int32 v) {
        bool isNegative = v < 0;
        uint32 value = isNegative ? -v : v;
        uint8 byte = ((value & 0x3F) << 2) | ((value > 0x3F) << 1) | isNegative;
        writeByte(byte);

        // Write out the rest of the bytes, if needed.
        value >>= 6;
        if (value == 0)
            return;
        writeUnsigned(value);
    }
    void writeFixedUint32(uint32 value) {
        writeByte(value & 0xFF);
        writeByte((value >> 8) & 0xFF);
        writeByte((value >> 16) & 0xFF);
        writeByte((value >> 24) & 0xFF);
    }
    size_t length() const {
        return buffer_.length();
    }
    uint8 *buffer() {
        return &buffer_[0];
    }
    const uint8 *buffer() const {
        return &buffer_[0];
    }
    bool oom() const {
        return !enoughMemory_;
    }
};

} // namespace ion
} // namespace js

#endif // jsion_compact_buffer_h__

