/* Copyright 2021 Mozilla Foundation
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

// ./test/core/custom.wast

// ./test/core/custom.wast:1
let $0 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\00\\24\\10" "a custom section" "this is the payload"
  "\\00\\20\\10" "a custom section" "this is payload"
  "\\00\\11\\10" "a custom section" ""
  "\\00\\10\\00" "" "this is payload"
  "\\00\\01\\00" "" ""
  "\\00\\24\\10" "\\00\\00custom sectio\\00" "this is the payload"
  "\\00\\24\\10" "\\ef\\bb\\bfa custom sect" "this is the payload"
  "\\00\\24\\10" "a custom sect\\e2\\8c\\a3" "this is the payload"
  "\\00\\1f\\16" "module within a module" "\\00asm" "\\01\\00\\00\\00"
)`);

// ./test/core/custom.wast:14
let $1 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\00\\0e\\06" "custom" "payload"
  "\\00\\0e\\06" "custom" "payload"
  "\\01\\01\\00"  ;; type section
  "\\00\\0e\\06" "custom" "payload"
  "\\00\\0e\\06" "custom" "payload"
  "\\02\\01\\00"  ;; import section
  "\\00\\0e\\06" "custom" "payload"
  "\\00\\0e\\06" "custom" "payload"
  "\\03\\01\\00"  ;; function section
  "\\00\\0e\\06" "custom" "payload"
  "\\00\\0e\\06" "custom" "payload"
  "\\04\\01\\00"  ;; table section
  "\\00\\0e\\06" "custom" "payload"
  "\\00\\0e\\06" "custom" "payload"
  "\\05\\01\\00"  ;; memory section
  "\\00\\0e\\06" "custom" "payload"
  "\\00\\0e\\06" "custom" "payload"
  "\\06\\01\\00"  ;; global section
  "\\00\\0e\\06" "custom" "payload"
  "\\00\\0e\\06" "custom" "payload"
  "\\07\\01\\00"  ;; export section
  "\\00\\0e\\06" "custom" "payload"
  "\\00\\0e\\06" "custom" "payload"
  "\\09\\01\\00"  ;; element section
  "\\00\\0e\\06" "custom" "payload"
  "\\00\\0e\\06" "custom" "payload"
  "\\0a\\01\\00"  ;; code section
  "\\00\\0e\\06" "custom" "payload"
  "\\00\\0e\\06" "custom" "payload"
  "\\0b\\01\\00"  ;; data section
  "\\00\\0e\\06" "custom" "payload"
  "\\00\\0e\\06" "custom" "payload"
)`);

// ./test/core/custom.wast:50
let $2 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\01\\07\\01\\60\\02\\7f\\7f\\01\\7f"                ;; type section
  "\\00\\1a\\06" "custom" "this is the payload"   ;; custom section
  "\\03\\02\\01\\00"                               ;; function section
  "\\07\\0a\\01\\06\\61\\64\\64\\54\\77\\6f\\00\\00"       ;; export section
  "\\0a\\09\\01\\07\\00\\20\\00\\20\\01\\6a\\0b"          ;; code section
  "\\00\\1b\\07" "custom2" "this is the payload"  ;; custom section
)`);

// ./test/core/custom.wast:60
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00"
  )`), `unexpected end`);

// ./test/core/custom.wast:68
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\00"
  )`), `unexpected end`);

// ./test/core/custom.wast:76
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\00\\00\\05\\01\\00\\07\\00\\00"
  )`), `unexpected end`);

// ./test/core/custom.wast:84
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\26\\10" "a custom section" "this is the payload"
  )`), `length out of bounds`);

// ./test/core/custom.wast:92
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00\\25\\10" "a custom section" "this is the payload"
    "\\00\\24\\10" "a custom section" "this is the payload"
  )`), `malformed section id`);

// ./test/core/custom.wast:101
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\07\\01\\60\\02\\7f\\7f\\01\\7f"                         ;; type section
    "\\00\\25\\10" "a custom section" "this is the payload"  ;; wrong length!
    "\\03\\02\\01\\00"                                        ;; function section
    "\\0a\\09\\01\\07\\00\\20\\00\\20\\01\\6a\\0b"                   ;; code section
    "\\00\\1b\\07" "custom2" "this is the payload"           ;; custom section
  )`), `function and code section have inconsistent lengths`);

// ./test/core/custom.wast:114
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm\\01\\00\\00\\00"
    "\\00asm\\01\\00\\00\\00"
  )`), `length out of bounds`);

// ./test/core/custom.wast:122
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\03\\01\\00\\01"                         ;; memory section
    "\\0c\\01\\02"                               ;; data count section (2 segments)
    "\\0b\\06\\01\\00\\41\\00\\0b\\00"                ;; data section (1 segment)
  )`), `data count and data section have inconsistent lengths`);
