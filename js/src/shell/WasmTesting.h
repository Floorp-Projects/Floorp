/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2020 Mozilla Foundation
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

#ifndef shell_wasm_h
#define shell_wasm_h

#include "wasm/WasmTypeDecls.h"

namespace js {
namespace wasm {

// Translate the textual representation of a wasm module (given by a
// char16_t array + length) into serialized bytes. If there is an error
// other than out-of-memory an error message string will be stored in 'error'.

[[nodiscard]] extern bool TextToBinary(const char16_t* text, size_t textLen,
                                       Bytes* bytes, UniqueChars* error);

}  // namespace wasm
}  // namespace js

#endif  // shell_wasm_h
