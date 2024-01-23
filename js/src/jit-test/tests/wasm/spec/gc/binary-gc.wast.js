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

// ./test/core/gc/binary-gc.wast

// ./test/core/gc/binary-gc.wast:1
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01"                     ;; Type section id
    "\\05"                     ;; Type section length
    "\\01"                     ;; Types vector length
    "\\5e"                     ;; Array type, -0x22
    "\\78"                     ;; Storage type: i8 or -0x08
    "\\02"                     ;; Mutability, should be 0 or 1, but isn't
  )`),
  `malformed mutability`,
);
