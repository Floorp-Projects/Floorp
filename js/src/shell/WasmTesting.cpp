/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2015 Mozilla Foundation
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

#include "shell/WasmTesting.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

#include "wasm/WasmTypes.h"

using namespace js;
using namespace js::wasm;

extern "C" {
bool wasm_text_to_binary(const char16_t* text, size_t text_len,
                         uint8_t** out_bytes, size_t* out_bytes_len,
                         uint8_t** out_error, size_t* out_error_len);
void wasm_code_offsets(const uint8_t* bytes, size_t bytes_len,
                       uint32_t** out_offsets, size_t* out_offset_len);
}  // extern "C"

bool wasm::TextToBinary(const char16_t* text, size_t textLen, Bytes* bytes,
                        UniqueChars* error) {
  uint8_t* outBytes = nullptr;
  size_t outBytesLength = 0;

  uint8_t* outError = nullptr;
  size_t outErrorLength = 0;

  bool result = wasm_text_to_binary(text, textLen, &outBytes, &outBytesLength,
                                    &outError, &outErrorLength);

  if (result) {
    MOZ_ASSERT(outBytes);
    MOZ_ASSERT(outBytesLength > 0);
    bytes->replaceRawBuffer(outBytes, outBytesLength);
    return true;
  }

  MOZ_ASSERT(outError);
  MOZ_ASSERT(outErrorLength > 0);
  *error = UniqueChars{(char*)outError};
  return false;
}

void wasm::CodeOffsets(const uint8_t* bytes, size_t bytesLen,
                       Uint32Vector* offsets) {
  uint32_t* outOffsets = nullptr;
  size_t outOffsetsLength = 0;

  wasm_code_offsets(bytes, bytesLen, &outOffsets, &outOffsetsLength);

  if (outOffsets) {
    MOZ_ASSERT(outOffsetsLength > 0);
    offsets->replaceRawBuffer(outOffsets, outOffsetsLength);
  } else {
    offsets->clear();
  }
}
