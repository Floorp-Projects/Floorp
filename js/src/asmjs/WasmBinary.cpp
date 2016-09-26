/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2016 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "asmjs/WasmBinary.h"

#include <stdarg.h>

#include "jsprf.h"
#include "asmjs/WasmTypes.h"

using namespace js;
using namespace js::wasm;

bool
Decoder::fail(const char* msg, ...) {
    va_list ap;
    va_start(ap, msg);
    UniqueChars str(JS_vsmprintf(msg, ap));
    va_end(ap);
    if (!str)
        return false;

    return fail(Move(str));
}

bool
Decoder::fail(UniqueChars msg) {
    UniqueChars strWithOffset(JS_smprintf("at offset %zu: %s", currentOffset(), msg.get()));
    if (!strWithOffset)
        return false;

    *error_ = Move(strWithOffset);
    return false;
}

bool
wasm::DecodePreamble(Decoder& d)
{
    uint32_t u32;
    if (!d.readFixedU32(&u32) || u32 != MagicNumber)
        return d.fail("failed to match magic number");

    if (!d.readFixedU32(&u32) || u32 != EncodingVersion)
        return d.fail("binary version 0x%" PRIx32 " does not match expected version 0x%" PRIx32,
                      u32, EncodingVersion);

    return true;
}

bool
wasm::EncodeLocalEntries(Encoder& e, const ValTypeVector& locals)
{
    uint32_t numLocalEntries = 0;
    ValType prev = ValType::Limit;
    for (ValType t : locals) {
        if (t != prev) {
            numLocalEntries++;
            prev = t;
        }
    }

    if (!e.writeVarU32(numLocalEntries))
        return false;

    if (numLocalEntries) {
        prev = locals[0];
        uint32_t count = 1;
        for (uint32_t i = 1; i < locals.length(); i++, count++) {
            if (prev != locals[i]) {
                if (!e.writeVarU32(count))
                    return false;
                if (!e.writeValType(prev))
                    return false;
                prev = locals[i];
                count = 0;
            }
        }
        if (!e.writeVarU32(count))
            return false;
        if (!e.writeValType(prev))
            return false;
    }

    return true;
}

bool
wasm::DecodeLocalEntries(Decoder& d, ValTypeVector* locals)
{
    uint32_t numLocalEntries;
    if (!d.readVarU32(&numLocalEntries))
        return false;

    for (uint32_t i = 0; i < numLocalEntries; i++) {
        uint32_t count;
        if (!d.readVarU32(&count))
            return false;

        if (MaxLocals - locals->length() < count)
            return false;

        ValType type;
        if (!d.readValType(&type))
            return false;

        if (!locals->appendN(type, count))
            return false;
    }

    return true;
}

bool
wasm::DecodeGlobalType(Decoder& d, ValType* type, uint32_t* flags)
{
    if (!d.readValType(type))
        return d.fail("bad global type");

    if (!d.readVarU32(flags))
        return d.fail("expected global flags");

    if (*flags & ~uint32_t(GlobalFlags::AllowedMask))
        return d.fail("unexpected bits set in global flags");

    return true;
}

bool
wasm::DecodeResizable(Decoder& d, ResizableLimits* limits)
{
    uint32_t flags;
    if (!d.readVarU32(&flags))
        return d.fail("expected flags");

    if (flags & ~uint32_t(ResizableFlags::AllowedMask))
        return d.fail("unexpected bits set in flags: %" PRIu32,
                      (flags & ~uint32_t(ResizableFlags::AllowedMask)));

    if (!(flags & uint32_t(ResizableFlags::Default)))
        return d.fail("currently, every memory/table must be declared default");

    if (!d.readVarU32(&limits->initial))
        return d.fail("expected initial length");

    if (flags & uint32_t(ResizableFlags::HasMaximum)) {
        uint32_t maximum;
        if (!d.readVarU32(&maximum))
            return d.fail("expected maximum length");

        if (limits->initial > maximum) {
            return d.fail("memory size minimum must not be greater than maximum; "
                          "maximum length %" PRIu32 " is less than initial length %" PRIu32,
                          maximum, limits->initial);
        }

        limits->maximum.emplace(maximum);
    }

    return true;
}

bool
wasm::DecodeUnknownSections(Decoder& d)
{
    while (!d.done()) {
        if (!d.skipUserDefinedSection())
            return false;
    }

    return true;
}
