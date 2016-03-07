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

#include "asmjs/WasmTypes.h"

using namespace js;
using namespace js::wasm;

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
