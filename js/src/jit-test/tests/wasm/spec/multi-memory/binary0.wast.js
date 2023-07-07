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

// ./test/core/multi-memory/binary0.wast

// ./test/core/multi-memory/binary0.wast:2
let $0 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\05\\07\\02"                          ;; Memory section with 2 entries
  "\\00\\82\\00"                          ;; no max, minimum 2
  "\\00\\82\\00"                          ;; no max, minimum 2
)`);

// ./test/core/multi-memory/binary0.wast:8
let $1 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\05\\13\\03"                          ;; Memory section with 3 entries
  "\\00\\83\\80\\80\\80\\00"                 ;; no max, minimum 3
  "\\00\\84\\80\\80\\80\\00"                 ;; no max, minimum 4
  "\\00\\85\\80\\80\\80\\00"                 ;; no max, minimum 5
)`);

// ./test/core/multi-memory/binary0.wast:16
let $2 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\05\\05\\02"                          ;; Memory section with 2 entries
  "\\00\\00"                             ;; no max, minimum 0
  "\\00\\00"                             ;; no max, minimum 0
  "\\0b\\06\\01"                          ;; Data section with 1 entry
  "\\00"                                ;; Memory index 0
  "\\41\\00\\0b\\00"                       ;; (i32.const 0) with contents ""
)`);

// ./test/core/multi-memory/binary0.wast:26
let $3 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\05\\05\\02"                          ;; Memory section with 2 entries
  "\\00\\00"                             ;; no max, minimum 0
  "\\00\\01"                             ;; no max, minimum 1
  "\\0b\\07\\01"                          ;; Data section with 1 entry
  "\\02\\01"                             ;; Memory index 1
  "\\41\\00\\0b\\00"                       ;; (i32.const 0) with contents ""
)`);

// ./test/core/multi-memory/binary0.wast:36
let $4 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\05\\05\\02"                          ;; Memory section with 2 entries
  "\\00\\00"                             ;; no max, minimum 0
  "\\00\\01"                             ;; no max, minimum 1
  "\\0b\\0a\\01"                          ;; Data section with 1 entry
  "\\02\\81\\80\\80\\00"                    ;; Memory index 1
  "\\41\\00\\0b\\00"                       ;; (i32.const 0) with contents ""
)`);

// ./test/core/multi-memory/binary0.wast:47
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\0a\\02"                          ;; Memory section with 2 entries
    "\\00\\01"                             ;; no max, minimum 1
    "\\00\\82\\80\\80\\80\\80\\00"              ;; no max, minimum 2 with one byte too many
  )`),
  `integer representation too long`,
);

// ./test/core/multi-memory/binary0.wast:58
assert_malformed(
  () => instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\05\\03\\02"                           ;; memory section with inconsistent count (1 declared, 0 given)
      "\\00\\00"                              ;; memory 0 (missed)
      ;; "\\00\\00"                           ;; memory 1 (missing)
  )`),
  `unexpected end of section or function`,
);
