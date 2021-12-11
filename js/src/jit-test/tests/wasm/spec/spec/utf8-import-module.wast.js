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

// ./test/core/utf8-import-module.wast

// ./test/core/utf8-import-module.wast:6
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0b"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\01\\80"                       ;; "\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:21
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0b"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\01\\8f"                       ;; "\\8f"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:36
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0b"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\01\\90"                       ;; "\\90"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:51
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0b"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\01\\9f"                       ;; "\\9f"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:66
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0b"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\01\\a0"                       ;; "\\a0"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:81
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0b"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\01\\bf"                       ;; "\\bf"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:98
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\c2\\80\\80"                 ;; "\\c2\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:113
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0b"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\01\\c2"                       ;; "\\c2"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:128
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0c"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\02\\c2\\2e"                    ;; "\\c2."
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:145
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0c"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\02\\c0\\80"                    ;; "\\c0\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:160
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0c"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\02\\c0\\bf"                    ;; "\\c0\\bf"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:175
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0c"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\02\\c1\\80"                    ;; "\\c1\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:190
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0c"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\02\\c1\\bf"                    ;; "\\c1\\bf"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:205
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0c"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\02\\c2\\00"                    ;; "\\c2\\00"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:220
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0c"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\02\\c2\\7f"                    ;; "\\c2\\7f"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:235
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0c"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\02\\c2\\c0"                    ;; "\\c2\\c0"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:250
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0c"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\02\\c2\\fd"                    ;; "\\c2\\fd"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:265
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0c"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\02\\df\\00"                    ;; "\\df\\00"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:280
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0c"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\02\\df\\7f"                    ;; "\\df\\7f"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:295
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0c"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\02\\df\\c0"                    ;; "\\df\\c0"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:310
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0c"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\02\\df\\fd"                    ;; "\\df\\fd"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:327
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\e1\\80\\80\\80"              ;; "\\e1\\80\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:342
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0c"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\02\\e1\\80"                    ;; "\\e1\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:357
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\e1\\80\\2e"                 ;; "\\e1\\80."
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:372
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0b"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\01\\e1"                       ;; "\\e1"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:387
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0c"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\02\\e1\\2e"                    ;; "\\e1."
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:404
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\e0\\00\\a0"                 ;; "\\e0\\00\\a0"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:419
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\e0\\7f\\a0"                 ;; "\\e0\\7f\\a0"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:434
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\e0\\80\\80"                 ;; "\\e0\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:449
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\e0\\80\\a0"                 ;; "\\e0\\80\\a0"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:464
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\e0\\9f\\a0"                 ;; "\\e0\\9f\\a0"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:479
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\e0\\9f\\bf"                 ;; "\\e0\\9f\\bf"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:494
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\e0\\c0\\a0"                 ;; "\\e0\\c0\\a0"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:509
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\e0\\fd\\a0"                 ;; "\\e0\\fd\\a0"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:524
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\e1\\00\\80"                 ;; "\\e1\\00\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:539
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\e1\\7f\\80"                 ;; "\\e1\\7f\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:554
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\e1\\c0\\80"                 ;; "\\e1\\c0\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:569
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\e1\\fd\\80"                 ;; "\\e1\\fd\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:584
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ec\\00\\80"                 ;; "\\ec\\00\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:599
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ec\\7f\\80"                 ;; "\\ec\\7f\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:614
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ec\\c0\\80"                 ;; "\\ec\\c0\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:629
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ec\\fd\\80"                 ;; "\\ec\\fd\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:644
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ed\\00\\80"                 ;; "\\ed\\00\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:659
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ed\\7f\\80"                 ;; "\\ed\\7f\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:674
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ed\\a0\\80"                 ;; "\\ed\\a0\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:689
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ed\\a0\\bf"                 ;; "\\ed\\a0\\bf"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:704
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ed\\bf\\80"                 ;; "\\ed\\bf\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:719
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ed\\bf\\bf"                 ;; "\\ed\\bf\\bf"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:734
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ed\\c0\\80"                 ;; "\\ed\\c0\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:749
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ed\\fd\\80"                 ;; "\\ed\\fd\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:764
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ee\\00\\80"                 ;; "\\ee\\00\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:779
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ee\\7f\\80"                 ;; "\\ee\\7f\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:794
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ee\\c0\\80"                 ;; "\\ee\\c0\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:809
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ee\\fd\\80"                 ;; "\\ee\\fd\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:824
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ef\\00\\80"                 ;; "\\ef\\00\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:839
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ef\\7f\\80"                 ;; "\\ef\\7f\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:854
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ef\\c0\\80"                 ;; "\\ef\\c0\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:869
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ef\\fd\\80"                 ;; "\\ef\\fd\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:886
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\e0\\a0\\00"                 ;; "\\e0\\a0\\00"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:901
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\e0\\a0\\7f"                 ;; "\\e0\\a0\\7f"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:916
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\e0\\a0\\c0"                 ;; "\\e0\\a0\\c0"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:931
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\e0\\a0\\fd"                 ;; "\\e0\\a0\\fd"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:946
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\e1\\80\\00"                 ;; "\\e1\\80\\00"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:961
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\e1\\80\\7f"                 ;; "\\e1\\80\\7f"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:976
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\e1\\80\\c0"                 ;; "\\e1\\80\\c0"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:991
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\e1\\80\\fd"                 ;; "\\e1\\80\\fd"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1006
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ec\\80\\00"                 ;; "\\ec\\80\\00"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1021
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ec\\80\\7f"                 ;; "\\ec\\80\\7f"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1036
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ec\\80\\c0"                 ;; "\\ec\\80\\c0"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1051
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ec\\80\\fd"                 ;; "\\ec\\80\\fd"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1066
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ed\\80\\00"                 ;; "\\ed\\80\\00"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1081
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ed\\80\\7f"                 ;; "\\ed\\80\\7f"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1096
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ed\\80\\c0"                 ;; "\\ed\\80\\c0"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1111
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ed\\80\\fd"                 ;; "\\ed\\80\\fd"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1126
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ee\\80\\00"                 ;; "\\ee\\80\\00"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1141
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ee\\80\\7f"                 ;; "\\ee\\80\\7f"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1156
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ee\\80\\c0"                 ;; "\\ee\\80\\c0"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1171
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ee\\80\\fd"                 ;; "\\ee\\80\\fd"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1186
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ef\\80\\00"                 ;; "\\ef\\80\\00"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1201
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ef\\80\\7f"                 ;; "\\ef\\80\\7f"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1216
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ef\\80\\c0"                 ;; "\\ef\\80\\c0"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1231
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\ef\\80\\fd"                 ;; "\\ef\\80\\fd"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1248
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0f"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\05\\f1\\80\\80\\80\\80"           ;; "\\f1\\80\\80\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1263
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\f1\\80\\80"                 ;; "\\f1\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1278
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f1\\80\\80\\23"              ;; "\\f1\\80\\80#"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1293
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0c"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\02\\f1\\80"                    ;; "\\f1\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1308
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\f1\\80\\23"                 ;; "\\f1\\80#"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1323
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0b"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\01\\f1"                       ;; "\\f1"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1338
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0c"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\02\\f1\\23"                    ;; "\\f1#"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1355
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f0\\00\\90\\90"              ;; "\\f0\\00\\90\\90"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1370
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f0\\7f\\90\\90"              ;; "\\f0\\7f\\90\\90"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1385
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f0\\80\\80\\80"              ;; "\\f0\\80\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1400
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f0\\80\\90\\90"              ;; "\\f0\\80\\90\\90"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1415
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f0\\8f\\90\\90"              ;; "\\f0\\8f\\90\\90"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1430
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f0\\8f\\bf\\bf"              ;; "\\f0\\8f\\bf\\bf"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1445
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f0\\c0\\90\\90"              ;; "\\f0\\c0\\90\\90"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1460
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f0\\fd\\90\\90"              ;; "\\f0\\fd\\90\\90"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1475
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f1\\00\\80\\80"              ;; "\\f1\\00\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1490
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f1\\7f\\80\\80"              ;; "\\f1\\7f\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1505
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f1\\c0\\80\\80"              ;; "\\f1\\c0\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1520
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f1\\fd\\80\\80"              ;; "\\f1\\fd\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1535
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f3\\00\\80\\80"              ;; "\\f3\\00\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1550
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f3\\7f\\80\\80"              ;; "\\f3\\7f\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1565
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f3\\c0\\80\\80"              ;; "\\f3\\c0\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1580
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f3\\fd\\80\\80"              ;; "\\f3\\fd\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1595
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f4\\00\\80\\80"              ;; "\\f4\\00\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1610
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f4\\7f\\80\\80"              ;; "\\f4\\7f\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1625
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f4\\90\\80\\80"              ;; "\\f4\\90\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1640
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f4\\bf\\80\\80"              ;; "\\f4\\bf\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1655
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f4\\c0\\80\\80"              ;; "\\f4\\c0\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1670
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f4\\fd\\80\\80"              ;; "\\f4\\fd\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1685
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f5\\80\\80\\80"              ;; "\\f5\\80\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1700
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f7\\80\\80\\80"              ;; "\\f7\\80\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1715
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f7\\bf\\bf\\bf"              ;; "\\f7\\bf\\bf\\bf"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1732
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f0\\90\\00\\90"              ;; "\\f0\\90\\00\\90"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1747
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f0\\90\\7f\\90"              ;; "\\f0\\90\\7f\\90"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1762
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f0\\90\\c0\\90"              ;; "\\f0\\90\\c0\\90"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1777
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f0\\90\\fd\\90"              ;; "\\f0\\90\\fd\\90"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1792
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f1\\80\\00\\80"              ;; "\\f1\\80\\00\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1807
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f1\\80\\7f\\80"              ;; "\\f1\\80\\7f\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1822
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f1\\80\\c0\\80"              ;; "\\f1\\80\\c0\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1837
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f1\\80\\fd\\80"              ;; "\\f1\\80\\fd\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1852
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f3\\80\\00\\80"              ;; "\\f3\\80\\00\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1867
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f3\\80\\7f\\80"              ;; "\\f3\\80\\7f\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1882
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f3\\80\\c0\\80"              ;; "\\f3\\80\\c0\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1897
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f3\\80\\fd\\80"              ;; "\\f3\\80\\fd\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1912
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f4\\80\\00\\80"              ;; "\\f4\\80\\00\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1927
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f4\\80\\7f\\80"              ;; "\\f4\\80\\7f\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1942
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f4\\80\\c0\\80"              ;; "\\f4\\80\\c0\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1957
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f4\\80\\fd\\80"              ;; "\\f4\\80\\fd\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1974
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f0\\90\\90\\00"              ;; "\\f0\\90\\90\\00"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:1989
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f0\\90\\90\\7f"              ;; "\\f0\\90\\90\\7f"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2004
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f0\\90\\90\\c0"              ;; "\\f0\\90\\90\\c0"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2019
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f0\\90\\90\\fd"              ;; "\\f0\\90\\90\\fd"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2034
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f1\\80\\80\\00"              ;; "\\f1\\80\\80\\00"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2049
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f1\\80\\80\\7f"              ;; "\\f1\\80\\80\\7f"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2064
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f1\\80\\80\\c0"              ;; "\\f1\\80\\80\\c0"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2079
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f1\\80\\80\\fd"              ;; "\\f1\\80\\80\\fd"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2094
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f3\\80\\80\\00"              ;; "\\f3\\80\\80\\00"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2109
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f3\\80\\80\\7f"              ;; "\\f3\\80\\80\\7f"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2124
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f3\\80\\80\\c0"              ;; "\\f3\\80\\80\\c0"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2139
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f3\\80\\80\\fd"              ;; "\\f3\\80\\80\\fd"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2154
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f4\\80\\80\\00"              ;; "\\f4\\80\\80\\00"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2169
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f4\\80\\80\\7f"              ;; "\\f4\\80\\80\\7f"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2184
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f4\\80\\80\\c0"              ;; "\\f4\\80\\80\\c0"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2199
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f4\\80\\80\\fd"              ;; "\\f4\\80\\80\\fd"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2216
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\10"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\06\\f8\\80\\80\\80\\80\\80"        ;; "\\f8\\80\\80\\80\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2231
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f8\\80\\80\\80"              ;; "\\f8\\80\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2246
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0f"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\05\\f8\\80\\80\\80\\23"           ;; "\\f8\\80\\80\\80#"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2261
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\f8\\80\\80"                 ;; "\\f8\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2276
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\f8\\80\\80\\23"              ;; "\\f8\\80\\80#"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2291
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0c"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\02\\f8\\80"                    ;; "\\f8\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2306
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\f8\\80\\23"                 ;; "\\f8\\80#"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2321
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0b"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\01\\f8"                       ;; "\\f8"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2336
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0c"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\02\\f8\\23"                    ;; "\\f8#"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2353
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0f"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\05\\f8\\80\\80\\80\\80"           ;; "\\f8\\80\\80\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2368
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0f"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\05\\fb\\bf\\bf\\bf\\bf"           ;; "\\fb\\bf\\bf\\bf\\bf"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2385
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\11"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\07\\fc\\80\\80\\80\\80\\80\\80"     ;; "\\fc\\80\\80\\80\\80\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2400
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0f"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\05\\fc\\80\\80\\80\\80"           ;; "\\fc\\80\\80\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2415
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\10"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\06\\fc\\80\\80\\80\\80\\23"        ;; "\\fc\\80\\80\\80\\80#"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2430
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\fc\\80\\80\\80"              ;; "\\fc\\80\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2445
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0f"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\05\\fc\\80\\80\\80\\23"           ;; "\\fc\\80\\80\\80#"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2460
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\fc\\80\\80"                 ;; "\\fc\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2475
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\fc\\80\\80\\23"              ;; "\\fc\\80\\80#"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2490
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0c"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\02\\fc\\80"                    ;; "\\fc\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2505
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0d"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\03\\fc\\80\\23"                 ;; "\\fc\\80#"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2520
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0b"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\01\\fc"                       ;; "\\fc"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2535
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0c"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\02\\fc\\23"                    ;; "\\fc#"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2552
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\10"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\06\\fc\\80\\80\\80\\80\\80"        ;; "\\fc\\80\\80\\80\\80\\80"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2567
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\10"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\06\\fd\\bf\\bf\\bf\\bf\\bf"        ;; "\\fd\\bf\\bf\\bf\\bf\\bf"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2584
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0b"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\01\\fe"                       ;; "\\fe"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2599
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0b"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\01\\ff"                       ;; "\\ff"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2614
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0c"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\02\\fe\\ff"                    ;; "\\fe\\ff"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2629
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\00\\00\\fe\\ff"              ;; "\\00\\00\\fe\\ff"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2644
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0c"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\02\\ff\\fe"                    ;; "\\ff\\fe"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);

// ./test/core/utf8-import-module.wast:2659
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\0e"                       ;; import section
    "\\01"                          ;; length 1
    "\\04\\74\\65\\73\\74"              ;; "test"
    "\\04\\ff\\fe\\00\\00"              ;; "\\ff\\fe\\00\\00"
    "\\03"                          ;; GlobalImport
    "\\7f"                          ;; i32
    "\\00"                          ;; immutable
  )`), `malformed UTF-8 encoding`);
