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

// ./test/core/binary-leb128.wast

// ./test/core/binary-leb128.wast:2
let $0 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\05\\04\\01"                          ;; Memory section with 1 entry
  "\\00\\82\\00"                          ;; no max, minimum 2
)`);

// ./test/core/binary-leb128.wast:7
let $1 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\05\\07\\01"                          ;; Memory section with 1 entry
  "\\00\\82\\80\\80\\80\\00"                 ;; no max, minimum 2
)`);

// ./test/core/binary-leb128.wast:12
let $2 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\05\\06\\01"                          ;; Memory section with 1 entry
  "\\01\\82\\00"                          ;; minimum 2
  "\\82\\00"                             ;; max 2
)`);

// ./test/core/binary-leb128.wast:18
let $3 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\05\\09\\01"                          ;; Memory section with 1 entry
  "\\01\\82\\00"                          ;; minimum 2
  "\\82\\80\\80\\80\\00"                    ;; max 2
)`);

// ./test/core/binary-leb128.wast:24
let $4 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\05\\03\\01"                          ;; Memory section with 1 entry
  "\\00\\00"                             ;; no max, minimum 0
  "\\0b\\07\\01"                          ;; Data section with 1 entry
  "\\80\\00"                             ;; Memory index 0, encoded with 2 bytes
  "\\41\\00\\0b\\00"                       ;; (i32.const 0) with contents ""
)`);

// ./test/core/binary-leb128.wast:32
let $5 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\04\\04\\01"                          ;; Table section with 1 entry
  "\\70\\00\\00"                          ;; no max, minimum 0, funcref
  "\\09\\07\\01"                          ;; Element section with 1 entry
  "\\80\\00"                             ;; Table index 0, encoded with 2 bytes
  "\\41\\00\\0b\\00"                       ;; (i32.const 0) with no elements
)`);

// ./test/core/binary-leb128.wast:40
let $6 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\00"                                ;; custom section
  "\\8a\\00"                             ;; section size 10, encoded with 2 bytes
  "\\01"                                ;; name byte count
  "1"                                  ;; name
  "23456789"                           ;; sequence of bytes
)`);

// ./test/core/binary-leb128.wast:48
let $7 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\00"                                ;; custom section
  "\\0b"                                ;; section size
  "\\88\\00"                             ;; name byte count 8, encoded with 2 bytes
  "12345678"                           ;; name
  "9"                                  ;; sequence of bytes
)`);

// ./test/core/binary-leb128.wast:56
let $8 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\01\\08\\01"                          ;; type section
  "\\60"                                ;; func type
  "\\82\\00"                             ;; num params 2, encoded with 2 bytes
  "\\7f\\7e"                             ;; param type
  "\\01"                                ;; num results
  "\\7f"                                ;; result type
)`);

// ./test/core/binary-leb128.wast:65
let $9 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\01\\08\\01"                          ;; type section
  "\\60"                                ;; func type
  "\\02"                                ;; num params
  "\\7f\\7e"                             ;; param type
  "\\81\\00"                             ;; num results 1, encoded with 2 bytes
  "\\7f"                                ;; result type
)`);

// ./test/core/binary-leb128.wast:74
let $10 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\01\\05\\01"                          ;; type section
  "\\60\\01\\7f\\00"                       ;; function type
  "\\02\\17\\01"                          ;; import section
  "\\88\\00"                             ;; module name length 8, encoded with 2 bytes
  "\\73\\70\\65\\63\\74\\65\\73\\74"           ;; module name
  "\\09"                                ;; entity name length
  "\\70\\72\\69\\6e\\74\\5f\\69\\33\\32"        ;; entity name
  "\\00"                                ;; import kind
  "\\00"                                ;; import signature index
)`);

// ./test/core/binary-leb128.wast:86
let $11 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\01\\05\\01"                          ;; type section
  "\\60\\01\\7f\\00"                       ;; function type
  "\\02\\17\\01"                          ;; import section
  "\\08"                                ;; module name length
  "\\73\\70\\65\\63\\74\\65\\73\\74"           ;; module name
  "\\89\\00"                             ;; entity name length 9, encoded with 2 bytes
  "\\70\\72\\69\\6e\\74\\5f\\69\\33\\32"        ;; entity name
  "\\00"                                ;; import kind
  "\\00"                                ;; import signature index
)`);

// ./test/core/binary-leb128.wast:98
let $12 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\01\\05\\01"                          ;; type section
  "\\60\\01\\7f\\00"                       ;; function type
  "\\02\\17\\01"                          ;; import section
  "\\08"                                ;; module name length
  "\\73\\70\\65\\63\\74\\65\\73\\74"           ;; module name
  "\\09"                                ;; entity name length 9
  "\\70\\72\\69\\6e\\74\\5f\\69\\33\\32"        ;; entity name
  "\\00"                                ;; import kind
  "\\80\\00"                             ;; import signature index, encoded with 2 bytes
)`);

// ./test/core/binary-leb128.wast:110
let $13 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\01\\04\\01"                          ;; type section
  "\\60\\00\\00"                          ;; function type
  "\\03\\03\\01"                          ;; function section
  "\\80\\00"                             ;; function 0 signature index, encoded with 2 bytes
  "\\0a\\04\\01"                          ;; code section
  "\\02\\00\\0b"                          ;; function body
)`);

// ./test/core/binary-leb128.wast:119
let $14 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\01\\04\\01"                          ;; type section
  "\\60\\00\\00"                          ;; fun type
  "\\03\\02\\01\\00"                       ;; function section
  "\\07\\07\\01"                          ;; export section
  "\\82\\00"                             ;; string length 2, encoded with 2 bytes
  "\\66\\31"                             ;; export name f1
  "\\00"                                ;; export kind
  "\\00"                                ;; export func index
  "\\0a\\04\\01"                          ;; code section
  "\\02\\00\\0b"                          ;; function body
)`);

// ./test/core/binary-leb128.wast:132
let $15 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\01\\04\\01"                          ;; type section
  "\\60\\00\\00"                          ;; fun type
  "\\03\\02\\01\\00"                       ;; function section
  "\\07\\07\\01"                          ;; export section
  "\\02"                                ;; string length 2
  "\\66\\31"                             ;; export name f1
  "\\00"                                ;; export kind
  "\\80\\00"                             ;; export func index, encoded with 2 bytes
  "\\0a\\04\\01"                          ;; code section
  "\\02\\00\\0b"                          ;; function body
)`);

// ./test/core/binary-leb128.wast:145
let $16 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\01\\04\\01"                          ;; type section
  "\\60\\00\\00"                          ;; fun type
  "\\03\\02\\01\\00"                       ;; function section
  "\\0a"                                ;; code section
  "\\05"                                ;; section size
  "\\81\\00"                             ;; num functions, encoded with 2 bytes
  "\\02\\00\\0b"                          ;; function body
)`);

// ./test/core/binary-leb128.wast:157
let $17 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\06\\07\\01"                          ;; Global section with 1 entry
  "\\7f\\00"                             ;; i32, immutable
  "\\41\\80\\00"                          ;; i32.const 0
  "\\0b"                                ;; end
)`);

// ./test/core/binary-leb128.wast:164
let $18 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\06\\07\\01"                          ;; Global section with 1 entry
  "\\7f\\00"                             ;; i32, immutable
  "\\41\\ff\\7f"                          ;; i32.const -1
  "\\0b"                                ;; end
)`);

// ./test/core/binary-leb128.wast:171
let $19 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\06\\0a\\01"                          ;; Global section with 1 entry
  "\\7f\\00"                             ;; i32, immutable
  "\\41\\80\\80\\80\\80\\00"                 ;; i32.const 0
  "\\0b"                                ;; end
)`);

// ./test/core/binary-leb128.wast:178
let $20 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\06\\0a\\01"                          ;; Global section with 1 entry
  "\\7f\\00"                             ;; i32, immutable
  "\\41\\ff\\ff\\ff\\ff\\7f"                 ;; i32.const -1
  "\\0b"                                ;; end
)`);

// ./test/core/binary-leb128.wast:186
let $21 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\06\\07\\01"                          ;; Global section with 1 entry
  "\\7e\\00"                             ;; i64, immutable
  "\\42\\80\\00"                          ;; i64.const 0 with unused bits set
  "\\0b"                                ;; end
)`);

// ./test/core/binary-leb128.wast:193
let $22 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\06\\07\\01"                          ;; Global section with 1 entry
  "\\7e\\00"                             ;; i64, immutable
  "\\42\\ff\\7f"                          ;; i64.const -1 with unused bits unset
  "\\0b"                                ;; end
)`);

// ./test/core/binary-leb128.wast:200
let $23 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\06\\0f\\01"                          ;; Global section with 1 entry
  "\\7e\\00"                             ;; i64, immutable
  "\\42\\80\\80\\80\\80\\80\\80\\80\\80\\80\\00"  ;; i64.const 0 with unused bits set
  "\\0b"                                ;; end
)`);

// ./test/core/binary-leb128.wast:207
let $24 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\06\\0f\\01"                          ;; Global section with 1 entry
  "\\7e\\00"                             ;; i64, immutable
  "\\42\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\7f"  ;; i64.const -1 with unused bits unset
  "\\0b"                                ;; end
)`);

// ./test/core/binary-leb128.wast:216
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\08\\01"                          ;; Memory section with 1 entry
    "\\00\\82\\80\\80\\80\\80\\00"              ;; no max, minimum 2 with one byte too many
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:224
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\0a\\01"                          ;; Memory section with 1 entry
    "\\01\\82\\00"                          ;; minimum 2
    "\\82\\80\\80\\80\\80\\00"                 ;; max 2 with one byte too many
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:233
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\03\\01"                          ;; Memory section with 1 entry
    "\\00\\00"                             ;; no max, minimum 0
    "\\0b\\0b\\01"                          ;; Data section with 1 entry
    "\\80\\80\\80\\80\\80\\00"                 ;; Memory index 0 with one byte too many
    "\\41\\00\\0b\\00"                       ;; (i32.const 0) with contents ""
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:244
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\04\\04\\01"                          ;; Table section with 1 entry
    "\\70\\00\\00"                          ;; no max, minimum 0, funcref
    "\\09\\0b\\01"                          ;; Element section with 1 entry
    "\\80\\80\\80\\80\\80\\00"                 ;; Table index 0 with one byte too many
    "\\41\\00\\0b\\00"                       ;; (i32.const 0) with no elements
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:255
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00"                                ;; custom section
    "\\83\\80\\80\\80\\80\\00"                 ;; section size 3 with one byte too many
    "\\01"                                ;; name byte count
    "1"                                  ;; name
    "2"                                  ;; sequence of bytes
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:266
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00"                                ;; custom section
    "\\0A"                                ;; section size
    "\\83\\80\\80\\80\\80\\00"                 ;; name byte count 3 with one byte too many
    "123"                                ;; name
    "4"                                  ;; sequence of bytes
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:277
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\0c\\01"                          ;; type section
    "\\60"                                ;; func type
    "\\82\\80\\80\\80\\80\\00"                 ;; num params 2 with one byte too many
    "\\7f\\7e"                             ;; param type
    "\\01"                                ;; num result
    "\\7f"                                ;; result type
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:289
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\08\\01"                          ;; type section
    "\\60"                                ;; func type
    "\\02"                                ;; num params
    "\\7f\\7e"                             ;; param type
    "\\81\\80\\80\\80\\80\\00"                 ;; num result 1 with one byte too many
    "\\7f"                                ;; result type
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:301
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\05\\01"                          ;; type section
    "\\60\\01\\7f\\00"                       ;; function type
    "\\02\\1b\\01"                          ;; import section
    "\\88\\80\\80\\80\\80\\00"                 ;; module name length 8 with one byte too many
    "\\73\\70\\65\\63\\74\\65\\73\\74"           ;; module name
    "\\09"                                ;; entity name length
    "\\70\\72\\69\\6e\\74\\5f\\69\\33\\32"        ;; entity name
    "\\00"                                ;; import kind
    "\\00"                                ;; import signature index
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:316
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\05\\01"                          ;; type section
    "\\60\\01\\7f\\00"                       ;; function type
    "\\02\\1b\\01"                          ;; import section
    "\\08"                                ;; module name length
    "\\73\\70\\65\\63\\74\\65\\73\\74"           ;; module name
    "\\89\\80\\80\\80\\80\\00"                 ;; entity name length 9 with one byte too many
    "\\70\\72\\69\\6e\\74\\5f\\69\\33\\32"        ;; entity name
    "\\00"                                ;; import kind
    "\\00"                                ;; import signature index
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:331
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\05\\01"                          ;; type section
    "\\60\\01\\7f\\00"                       ;; function type
    "\\02\\1b\\01"                          ;; import section
    "\\08"                                ;; module name length
    "\\73\\70\\65\\63\\74\\65\\73\\74"           ;; module name
    "\\09"                                ;; entity name length 9
    "\\70\\72\\69\\6e\\74\\5f\\69\\33\\32"        ;; entity name
    "\\00"                                ;; import kind
    "\\80\\80\\80\\80\\80\\00"                 ;; import signature index 0 with one byte too many
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:346
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01"                          ;; type section
    "\\60\\00\\00"                          ;; function type
    "\\03\\03\\01"                          ;; function section
    "\\80\\80\\80\\80\\80\\00"                 ;; function 0 signature index with one byte too many
    "\\0a\\04\\01"                          ;; code section
    "\\02\\00\\0b"                          ;; function body
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:358
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01"                          ;; type section
    "\\60\\00\\00"                          ;; fun type
    "\\03\\02\\01\\00"                       ;; function section
    "\\07\\0b\\01"                          ;; export section
    "\\82\\80\\80\\80\\80\\00"                 ;; string length 2 with one byte too many
    "\\66\\31"                             ;; export name f1
    "\\00"                                ;; export kind
    "\\00"                                ;; export func index
    "\\0a\\04\\01"                          ;; code section
    "\\02\\00\\0b"                          ;; function body
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:374
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01"                          ;; type section
    "\\60\\00\\00"                          ;; fun type
    "\\03\\02\\01\\00"                       ;; function section
    "\\07\\0b\\01"                          ;; export section
    "\\02"                                ;; string length 2
    "\\66\\31"                             ;; export name f1
    "\\00"                                ;; export kind
    "\\80\\80\\80\\80\\80\\00"                 ;; export func index 0 with one byte too many
    "\\0a\\04\\01"                          ;; code section
    "\\02\\00\\0b"                          ;; function body
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:390
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01"                          ;; type section
    "\\60\\00\\00"                          ;; fun type
    "\\03\\02\\01\\00"                       ;; function section
    "\\0a"                                ;; code section
    "\\05"                                ;; section size
    "\\81\\80\\80\\80\\80\\00"                 ;; num functions 1 with one byte too many
    "\\02\\00\\0b"                          ;; function body
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:403
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\05\\03\\01\\00\\01"          ;; Memory section
    "\\0a\\11\\01"                ;; Code section
    ;; function 0
    "\\0f\\01\\01"                ;; local type count
    "\\7f"                      ;; i32
    "\\41\\00"                   ;; i32.const 0
    "\\28"                      ;; i32.load
    "\\02"                      ;; alignment 2
    "\\82\\80\\80\\80\\80\\00"       ;; offset 2 with one byte too many
    "\\1a"                      ;; drop
    "\\0b"                      ;; end
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:422
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\05\\03\\01\\00\\01"          ;; Memory section
    "\\0a\\11\\01"                ;; Code section
    ;; function 0
    "\\0f\\01\\01"                ;; local type count
    "\\7f"                      ;; i32
    "\\41\\00"                   ;; i32.const 0
    "\\28"                      ;; i32.load
    "\\82\\80\\80\\80\\80\\00"       ;; alignment 2 with one byte too many
    "\\00"                      ;; offset 0
    "\\1a"                      ;; drop
    "\\0b"                      ;; end
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:441
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\05\\03\\01\\00\\01"          ;; Memory section
    "\\0a\\12\\01"                ;; Code section
    ;; function 0
    "\\10\\01\\01"                ;; local type count
    "\\7f"                      ;; i32
    "\\41\\00"                   ;; i32.const 0
    "\\41\\03"                   ;; i32.const 3
    "\\36"                      ;; i32.store
    "\\82\\80\\80\\80\\80\\00"       ;; alignment 2 with one byte too many
    "\\03"                      ;; offset 3
    "\\0b"                      ;; end
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:460
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\05\\03\\01\\00\\01"          ;; Memory section
    "\\0a\\12\\01"                ;; Code section
    ;; function 0
    "\\10\\01\\01"                ;; local type count
    "\\7f"                      ;; i32
    "\\41\\00"                   ;; i32.const 0
    "\\41\\03"                   ;; i32.const 3
    "\\36"                      ;; i32.store
    "\\02"                      ;; alignment 2
    "\\82\\80\\80\\80\\80\\00"       ;; offset 2 with one byte too many
    "\\0b"                      ;; end
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:481
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0b\\01"                          ;; Global section with 1 entry
    "\\7f\\00"                             ;; i32, immutable
    "\\41\\80\\80\\80\\80\\80\\00"              ;; i32.const 0 with one byte too many
    "\\0b"                                ;; end
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:491
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0b\\01"                          ;; Global section with 1 entry
    "\\7f\\00"                             ;; i32, immutable
    "\\41\\ff\\ff\\ff\\ff\\ff\\7f"              ;; i32.const -1 with one byte too many
    "\\0b"                                ;; end
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:502
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\10\\01"                          ;; Global section with 1 entry
    "\\7e\\00"                             ;; i64, immutable
    "\\42\\80\\80\\80\\80\\80\\80\\80\\80\\80\\80\\00"  ;; i64.const 0 with one byte too many
    "\\0b"                                ;; end
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:512
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\10\\01"                          ;; Global section with 1 entry
    "\\7e\\00"                             ;; i64, immutable
    "\\42\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\7f"  ;; i64.const -1 with one byte too many
    "\\0b"                                ;; end
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:524
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\07\\01"                          ;; Memory section with 1 entry
    "\\00\\82\\80\\80\\80\\70"                 ;; no max, minimum 2 with unused bits set
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:532
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\07\\01"                          ;; Memory section with 1 entry
    "\\00\\82\\80\\80\\80\\40"                 ;; no max, minimum 2 with some unused bits set
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:540
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\09\\01"                          ;; Memory section with 1 entry
    "\\01\\82\\00"                          ;; minimum 2
    "\\82\\80\\80\\80\\10"                    ;; max 2 with unused bits set
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:549
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\09\\01"                          ;; Memory section with 1 entry
    "\\01\\82\\00"                          ;; minimum 2
    "\\82\\80\\80\\80\\40"                    ;; max 2 with some unused bits set
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:558
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\03\\01"                          ;; Memory section with 1 entry
    "\\00\\00"                             ;; no max, minimum 0
    "\\0b\\0a\\01"                          ;; Data section with 1 entry
    "\\80\\80\\80\\80\\10"                    ;; Memory index 0 with unused bits set
    "\\41\\00\\0b\\00"                       ;; (i32.const 0) with contents ""
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:569
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\04\\04\\01"                          ;; Table section with 1 entry
    "\\70\\00\\00"                          ;; no max, minimum 0, funcref
    "\\09\\0a\\01"                          ;; Element section with 1 entry
    "\\80\\80\\80\\80\\10"                    ;; Table index 0 with unused bits set
    "\\41\\00\\0b\\00"                       ;; (i32.const 0) with no elements
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:580
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00"                                ;; custom section
    "\\83\\80\\80\\80\\10"                    ;; section size 3 with unused bits set
    "\\01"                                ;; name byte count
    "1"                                  ;; name
    "2"                                  ;; sequence of bytes
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:591
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00"                                ;; custom section
    "\\09"                                ;; section size
    "\\83\\80\\80\\80\\40"                    ;; name byte count 3 with unused bits set
    "123"                                ;; name
    "4"                                  ;; sequence of bytes
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:602
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\0b\\01"                          ;; type section
    "\\60"                                ;; func type
    "\\82\\80\\80\\80\\10"                    ;; num params 2 with unused bits set
    "\\7f\\7e"                             ;; param type
    "\\01"                                ;; num result
    "\\7f"                                ;; result type
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:614
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\0b\\01"                          ;; type section
    "\\60"                                ;; func type
    "\\02"                                ;; num params
    "\\7f\\7e"                             ;; param type
    "\\81\\80\\80\\80\\40"                    ;; num result 1 with unused bits set
    "\\7f"                                ;; result type
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:626
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\05\\01"                          ;; type section
    "\\60\\01\\7f\\00"                       ;; function type
    "\\02\\1a\\01"                          ;; import section
    "\\88\\80\\80\\80\\10"                    ;; module name length 8 with unused bits set
    "\\73\\70\\65\\63\\74\\65\\73\\74"           ;; module name
    "\\09"                                ;; entity name length
    "\\70\\72\\69\\6e\\74\\5f\\69\\33\\32"        ;; entity name
    "\\00"                                ;; import kind
    "\\00"                                ;; import signature index
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:641
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\05\\01"                          ;; type section
    "\\60\\01\\7f\\00"                       ;; function type
    "\\02\\1a\\01"                          ;; import section
    "\\08"                                ;; module name length
    "\\73\\70\\65\\63\\74\\65\\73\\74"           ;; module name
    "\\89\\80\\80\\80\\40"                    ;; entity name length 9 with unused bits set
    "\\70\\72\\69\\6e\\74\\5f\\69\\33\\32"        ;; entity name
    "\\00"                                ;; import kind
    "\\00"                                ;; import signature index
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:656
assert_malformed(() =>
  instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\01\\05\\01"                          ;; type section
  "\\60\\01\\7f\\00"                       ;; function type
  "\\02\\1a\\01"                          ;; import section
  "\\08"                                ;; module name length
  "\\73\\70\\65\\63\\74\\65\\73\\74"           ;; module name
  "\\09"                                ;; entity name length 9
  "\\70\\72\\69\\6e\\74\\5f\\69\\33\\32"        ;; entity name
  "\\00"                                ;; import kind
  "\\80\\80\\80\\80\\10"                    ;; import signature index 0 with unused bits set
)`), `integer too large`);

// ./test/core/binary-leb128.wast:671
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01"                          ;; type section
    "\\60\\00\\00"                          ;; function type
    "\\03\\06\\01"                          ;; function section
    "\\80\\80\\80\\80\\10"                    ;; function 0 signature index with unused bits set
    "\\0a\\04\\01"                          ;; code section
    "\\02\\00\\0b"                          ;; function body
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:684
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01"                          ;; type section
    "\\60\\00\\00"                          ;; fun type
    "\\03\\02\\01\\00"                       ;; function section
    "\\07\\0a\\01"                          ;; export section
    "\\82\\80\\80\\80\\10"                    ;; string length 2 with unused bits set
    "\\66\\31"                             ;; export name f1
    "\\00"                                ;; export kind
    "\\00"                                ;; export func index
    "\\0a\\04\\01"                          ;; code section
    "\\02\\00\\0b"                          ;; function body
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:700
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01"                          ;; type section
    "\\60\\00\\00"                          ;; fun type
    "\\03\\02\\01\\00"                       ;; function section
    "\\07\\0a\\01"                          ;; export section
    "\\02"                                ;; string length 2
    "\\66\\31"                             ;; export name f1
    "\\00"                                ;; export kind
    "\\80\\80\\80\\80\\10"                    ;; export func index with unused bits set
    "\\0a\\04\\01"                          ;; code section
    "\\02\\00\\0b"                          ;; function body
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:716
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01"                          ;; type section
    "\\60\\00\\00"                          ;; fun type
    "\\03\\02\\01\\00"                       ;; function section
    "\\0a"                                ;; code section
    "\\08"                                ;; section size
    "\\81\\80\\80\\80\\10"                    ;; num functions 1 with unused bits set
    "\\02\\00\\0b"                          ;; function body
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:729
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\05\\03\\01\\00\\01"          ;; Memory section
    "\\0a\\10\\01"                ;; Code section
    ;; function 0
    "\\0e\\01\\01"                ;; local type count
    "\\7f"                      ;; i32
    "\\41\\00"                   ;; i32.const 0
    "\\28"                      ;; i32.load
    "\\02"                      ;; alignment 2
    "\\82\\80\\80\\80\\10"          ;; offset 2 with unused bits set
    "\\1a"                      ;; drop
    "\\0b"                      ;; end
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:748
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\05\\03\\01\\00\\01"          ;; Memory section
    "\\0a\\10\\01"                ;; Code section
    ;; function 0
    "\\0e\\01\\01"                ;; local type count
    "\\7f"                      ;; i32
    "\\41\\00"                   ;; i32.const 0
    "\\28"                      ;; i32.load
    "\\02"                      ;; alignment 2
    "\\82\\80\\80\\80\\40"          ;; offset 2 with some unused bits set
    "\\1a"                      ;; drop
    "\\0b"                      ;; end
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:767
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\05\\03\\01\\00\\01"          ;; Memory section
    "\\0a\\10\\01"                ;; Code section
    "\\0e\\01\\01"                ;; local type count
    "\\7f"                      ;; i32
    "\\41\\00"                   ;; i32.const 0
    "\\28"                      ;; i32.load
    "\\82\\80\\80\\80\\10"          ;; alignment 2 with unused bits set
    "\\00"                      ;; offset 0
    "\\1a"                      ;; drop
    "\\0b"                      ;; end
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:785
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\05\\03\\01\\00\\01"          ;; Memory section
    "\\0a\\10\\01"                ;; Code section
    ;; function 0
    "\\0e\\01\\01"                ;; local type count
    "\\7f"                      ;; i32
    "\\41\\00"                   ;; i32.const 0
    "\\28"                      ;; i32.load
    "\\82\\80\\80\\80\\40"          ;; alignment 2 with some unused bits set
    "\\00"                      ;; offset 0
    "\\1a"                      ;; drop
    "\\0b"                      ;; end
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:804
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\05\\03\\01\\00\\01"          ;; Memory section
    "\\0a\\11\\01"                ;; Code section
    ;; function 0
    "\\0f\\01\\01"                ;; local type count
    "\\7f"                      ;; i32
    "\\41\\00"                   ;; i32.const 0
    "\\41\\03"                   ;; i32.const 3
    "\\36"                      ;; i32.store
    "\\82\\80\\80\\80\\10"          ;; alignment 2 with unused bits set
    "\\03"                      ;; offset 3
    "\\0b"                      ;; end
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:823
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\05\\03\\01\\00\\01"          ;; Memory section
    "\\0a\\11\\01"                ;; Code section
    ;; function 0
    "\\0f\\01\\01"                ;; local type count
    "\\7f"                      ;; i32
    "\\41\\00"                   ;; i32.const 0
    "\\41\\03"                   ;; i32.const 3
    "\\36"                      ;; i32.store
    "\\82\\80\\80\\80\\40"          ;; alignment 2 with some unused bits set
    "\\03"                      ;; offset 3
    "\\0b"                      ;; end
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:842
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\05\\03\\01\\00\\01"          ;; Memory section
    "\\0a\\11\\01"                ;; Code section
    ;; function 0
    "\\0f\\01\\01"                ;; local type count
    "\\7f"                      ;; i32
    "\\41\\00"                   ;; i32.const 0
    "\\41\\03"                   ;; i32.const 3
    "\\36"                      ;; i32.store
    "\\03"                      ;; alignment 2
    "\\82\\80\\80\\80\\10"          ;; offset 2 with unused bits set
    "\\0b"                      ;; end
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:861
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\05\\03\\01\\00\\01"          ;; Memory section
    "\\0a\\11\\01"                ;; Code section

    ;; function 0
    "\\0f\\01\\01"                ;; local type count
    "\\7f"                      ;; i32
    "\\41\\00"                   ;; i32.const 0
    "\\41\\03"                   ;; i32.const 3
    "\\36"                      ;; i32.store
    "\\02"                      ;; alignment 2
    "\\82\\80\\80\\80\\40"          ;; offset 2 with some unused bits set
    "\\0b"                      ;; end
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:883
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0a\\01"                          ;; Global section with 1 entry
    "\\7f\\00"                             ;; i32, immutable
    "\\41\\80\\80\\80\\80\\70"                 ;; i32.const 0 with unused bits set
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:893
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0a\\01"                          ;; Global section with 1 entry
    "\\7f\\00"                             ;; i32, immutable
    "\\41\\ff\\ff\\ff\\ff\\0f"                 ;; i32.const -1 with unused bits unset
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:903
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0a\\01"                          ;; Global section with 1 entry
    "\\7f\\00"                             ;; i32, immutable
    "\\41\\80\\80\\80\\80\\1f"                 ;; i32.const 0 with some unused bits set
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:913
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0a\\01"                          ;; Global section with 1 entry
    "\\7f\\00"                             ;; i32, immutable
    "\\41\\ff\\ff\\ff\\ff\\4f"                 ;; i32.const -1 with some unused bits unset
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:924
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0f\\01"                          ;; Global section with 1 entry
    "\\7e\\00"                             ;; i64, immutable
    "\\42\\80\\80\\80\\80\\80\\80\\80\\80\\80\\7e"  ;; i64.const 0 with unused bits set
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:934
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0f\\01"                          ;; Global section with 1 entry
    "\\7e\\00"                             ;; i64, immutable
    "\\42\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\01"  ;; i64.const -1 with unused bits unset
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:944
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0f\\01"                          ;; Global section with 1 entry
    "\\7e\\00"                             ;; i64, immutable
    "\\42\\80\\80\\80\\80\\80\\80\\80\\80\\80\\02"  ;; i64.const 0 with some unused bits set
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:954
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0f\\01"                          ;; Global section with 1 entry
    "\\7e\\00"                             ;; i64, immutable
    "\\42\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\41"  ;; i64.const -1 with some unused bits unset
    "\\0b"                                ;; end
  )`), `integer too large`);
