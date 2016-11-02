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

#ifndef wasm_binary_format_h
#define wasm_binary_format_h

#include "wasm/WasmBinary.h"
#include "wasm/WasmTypes.h"

namespace js {
namespace wasm {

// Reusable macro encoding/decoding functions reused by both the two
// encoders (AsmJS/WasmTextToBinary) and all the decoders
// (WasmCompile/WasmIonCompile/WasmBaselineCompile/WasmBinaryToText).

MOZ_MUST_USE bool
DecodePreamble(Decoder& d);

MOZ_MUST_USE bool
EncodeLocalEntries(Encoder& d, const ValTypeVector& locals);

MOZ_MUST_USE bool
DecodeLocalEntries(Decoder& d, ValTypeVector* locals);

MOZ_MUST_USE bool
DecodeGlobalType(Decoder& d, ValType* type, bool* isMutable);

MOZ_MUST_USE bool
DecodeInitializerExpression(Decoder& d, const GlobalDescVector& globals, ValType expected,
                            InitExpr* init);

MOZ_MUST_USE bool
DecodeLimits(Decoder& d, Limits* limits);

MOZ_MUST_USE bool
DecodeUnknownSections(Decoder& d);

MOZ_MUST_USE bool
DecodeDataSection(Decoder& d, bool usesMemory, uint32_t minMemoryByteLength,
                  const GlobalDescVector& globals, DataSegmentVector* segments);

MOZ_MUST_USE bool
DecodeMemoryLimits(Decoder& d, bool hasMemory, Limits* memory);

MOZ_MUST_USE bool
DecodeMemorySection(Decoder& d, bool hasMemory, Limits* memory, bool* present);

} // namespace wasm
} // namespace js

#endif // wasm_binary_format_h
