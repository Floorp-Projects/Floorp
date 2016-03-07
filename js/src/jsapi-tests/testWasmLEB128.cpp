/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>

#include "asmjs/WasmBinary.h"

#include "jsapi-tests/tests.h"

static bool WriteValidBytes(js::wasm::Encoder& encoder, bool* passed)
{
    *passed = false;
    if (!encoder.empty())
        return true;

    // These remain the same under LEB128 unsigned encoding
    if (!encoder.writeVarU32(0x0) ||
        !encoder.writeVarU32(0x1) ||
        !encoder.writeVarU32(0x42))
    {
        return false;
    }

    // 0x01 0x80
    if (!encoder.writeVarU32(0x80))
        return false;

    // 0x03 0x80
    if (!encoder.writeVarU32(0x180))
        return false;

    if (encoder.empty())
        return true;
    if (encoder.currentOffset() != 7)
        return true;
    *passed = true;
    return true;
}

BEGIN_TEST(testWasmLEB128_encoding)
{
    using namespace js;
    using namespace wasm;

    Bytes bytes;
    Encoder encoder(bytes);

    bool passed;
    if (!WriteValidBytes(encoder, &passed))
        return false;
    CHECK(passed);

    size_t i = 0;
    CHECK(bytes[i++] == 0x0);
    CHECK(bytes[i++] == 0x1);
    CHECK(bytes[i++] == 0x42);

    CHECK(bytes[i++] == 0x80);
    CHECK(bytes[i++] == 0x01);

    CHECK(bytes[i++] == 0x80);
    CHECK(bytes[i++] == 0x03);

    if (i + 1 < bytes.length())
        CHECK(bytes[i++] == 0x00);
    return true;
}
END_TEST(testWasmLEB128_encoding)

BEGIN_TEST(testWasmLEB128_valid_decoding)
{
    using namespace js;
    using namespace wasm;

    Bytes bytes;
    if (!bytes.append(0x0) || !bytes.append(0x1) || !bytes.append(0x42))
        return false;

    if (!bytes.append(0x80) || !bytes.append(0x01))
        return false;

    if (!bytes.append(0x80) || !bytes.append(0x03))
        return false;

    {
        // Fallible decoding
        Decoder decoder(bytes);
        uint32_t value;

        CHECK(decoder.readVarU32(&value) && value == 0x0);
        CHECK(decoder.readVarU32(&value) && value == 0x1);
        CHECK(decoder.readVarU32(&value) && value == 0x42);
        CHECK(decoder.readVarU32(&value) && value == 0x80);
        CHECK(decoder.readVarU32(&value) && value == 0x180);

        CHECK(decoder.done());
    }

    {
        // Infallible decoding
        Decoder decoder(bytes);
        uint32_t value;

        value = decoder.uncheckedReadVarU32();
        CHECK(value == 0x0);
        value = decoder.uncheckedReadVarU32();
        CHECK(value == 0x1);
        value = decoder.uncheckedReadVarU32();
        CHECK(value == 0x42);
        value = decoder.uncheckedReadVarU32();
        CHECK(value == 0x80);
        value = decoder.uncheckedReadVarU32();
        CHECK(value == 0x180);

        CHECK(decoder.done());
    }
    return true;
}
END_TEST(testWasmLEB128_valid_decoding)

BEGIN_TEST(testWasmLEB128_invalid_decoding)
{
    using namespace js;
    using namespace wasm;

    Bytes bytes;
    // Fill bits as per 28 encoded bits
    if (!bytes.append(0x80) || !bytes.append(0x80) || !bytes.append(0x80) || !bytes.append(0x80))
        return false;

    // Test last valid values
    if (!bytes.append(0x00))
        return false;

    for (uint8_t i = 0; i < 0x0F; i++) {
        bytes[4] = i;

        {
            Decoder decoder(bytes);
            uint32_t value;
            CHECK(decoder.readVarU32(&value));
            CHECK(value == uint32_t(i << 28));
            CHECK(decoder.done());
        }

        {
            Decoder decoder(bytes);
            uint32_t value = decoder.uncheckedReadVarU32();
            CHECK(value == uint32_t(i << 28));
            CHECK(decoder.done());
        }
    }

    // Test all invalid values of the same size
    for (uint8_t i = 0x10; i < 0xF0; i++) {
        bytes[4] = i;

        Decoder decoder(bytes);
        uint32_t value;
        CHECK(!decoder.readVarU32(&value));
    }

    return true;
}
END_TEST(testWasmLEB128_invalid_decoding)
