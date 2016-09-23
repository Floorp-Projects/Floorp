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

#include "asmjs/WasmTextUtils.h"

#include "asmjs/WasmBinary.h"
#include "vm/StringBuffer.h"

using namespace js;
using namespace js::jit;

using mozilla::IsNaN;

template<size_t base>
bool
js::wasm::RenderInBase(StringBuffer& sb, uint64_t num)
{
    uint64_t n = num;
    uint64_t pow = 1;
    while (n) {
        pow *= base;
        n /= base;
    }
    pow /= base;

    n = num;
    while (pow) {
        if (!sb.append("0123456789abcdef"[n / pow]))
            return false;
        n -= (n / pow) * pow;
        pow /= base;
    }

    return true;
}

template bool js::wasm::RenderInBase<10>(StringBuffer& sb, uint64_t num);

template<class T>
bool
js::wasm::RenderNaN(StringBuffer& sb, Raw<T> num)
{
    typedef typename mozilla::SelectTrait<T> Traits;

    MOZ_ASSERT(IsNaN(num.fp()));

    if ((num.bits() & Traits::kSignBit) && !sb.append("-"))
        return false;
    if (!sb.append("nan"))
        return false;

    typename Traits::Bits payload = num.bits() & Traits::kSignificandBits;
    // Only render the payload if it's not the spec's default NaN.
    if (payload == ((Traits::kSignificandBits + 1) >> 1))
        return true;

    return sb.append(":0x") &&
           RenderInBase<16>(sb, payload);
}

template MOZ_MUST_USE bool js::wasm::RenderNaN(StringBuffer& b, Raw<float> num);
template MOZ_MUST_USE bool js::wasm::RenderNaN(StringBuffer& b, Raw<double> num);
