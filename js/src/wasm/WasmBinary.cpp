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

#include "wasm/WasmBinary.h"

#include <stdarg.h>

#include "jsprf.h"
#include "wasm/WasmTypes.h"

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
    MOZ_ASSERT(error_);
    UniqueChars strWithOffset(JS_smprintf("at offset %" PRIuSIZE ": %s", currentOffset(), msg.get()));
    if (!strWithOffset)
        return false;

    *error_ = Move(strWithOffset);
    return false;
}
