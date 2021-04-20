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
  "\\09\\09\\01"                          ;; Element section with 1 entry
  "\\02"                                ;; Element with explicit table index
  "\\80\\00"                             ;; Table index 0, encoded with 2 bytes
  "\\41\\00\\0b\\00\\00"                    ;; (i32.const 0) with no elements
)`);

// ./test/core/binary-leb128.wast:41
let $6 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\00"                                ;; custom section
  "\\8a\\00"                             ;; section size 10, encoded with 2 bytes
  "\\01"                                ;; name byte count
  "1"                                  ;; name
  "23456789"                           ;; sequence of bytes
)`);

// ./test/core/binary-leb128.wast:49
let $7 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\00"                                ;; custom section
  "\\0b"                                ;; section size
  "\\88\\00"                             ;; name byte count 8, encoded with 2 bytes
  "12345678"                           ;; name
  "9"                                  ;; sequence of bytes
)`);

// ./test/core/binary-leb128.wast:57
let $8 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\01\\08\\01"                          ;; type section
  "\\60"                                ;; func type
  "\\82\\00"                             ;; num params 2, encoded with 2 bytes
  "\\7f\\7e"                             ;; param type
  "\\01"                                ;; num results
  "\\7f"                                ;; result type
)`);

// ./test/core/binary-leb128.wast:66
let $9 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\01\\08\\01"                          ;; type section
  "\\60"                                ;; func type
  "\\02"                                ;; num params
  "\\7f\\7e"                             ;; param type
  "\\81\\00"                             ;; num results 1, encoded with 2 bytes
  "\\7f"                                ;; result type
)`);

// ./test/core/binary-leb128.wast:75
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

// ./test/core/binary-leb128.wast:87
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

// ./test/core/binary-leb128.wast:99
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

// ./test/core/binary-leb128.wast:111
let $13 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\01\\04\\01"                          ;; type section
  "\\60\\00\\00"                          ;; function type
  "\\03\\03\\01"                          ;; function section
  "\\80\\00"                             ;; function 0 signature index, encoded with 2 bytes
  "\\0a\\04\\01"                          ;; code section
  "\\02\\00\\0b"                          ;; function body
)`);

// ./test/core/binary-leb128.wast:120
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

// ./test/core/binary-leb128.wast:133
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

// ./test/core/binary-leb128.wast:146
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

// ./test/core/binary-leb128.wast:158
let $17 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\06\\07\\01"                          ;; Global section with 1 entry
  "\\7f\\00"                             ;; i32, immutable
  "\\41\\80\\00"                          ;; i32.const 0
  "\\0b"                                ;; end
)`);

// ./test/core/binary-leb128.wast:165
let $18 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\06\\07\\01"                          ;; Global section with 1 entry
  "\\7f\\00"                             ;; i32, immutable
  "\\41\\ff\\7f"                          ;; i32.const -1
  "\\0b"                                ;; end
)`);

// ./test/core/binary-leb128.wast:172
let $19 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\06\\0a\\01"                          ;; Global section with 1 entry
  "\\7f\\00"                             ;; i32, immutable
  "\\41\\80\\80\\80\\80\\00"                 ;; i32.const 0
  "\\0b"                                ;; end
)`);

// ./test/core/binary-leb128.wast:179
let $20 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\06\\0a\\01"                          ;; Global section with 1 entry
  "\\7f\\00"                             ;; i32, immutable
  "\\41\\ff\\ff\\ff\\ff\\7f"                 ;; i32.const -1
  "\\0b"                                ;; end
)`);

// ./test/core/binary-leb128.wast:187
let $21 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\06\\07\\01"                          ;; Global section with 1 entry
  "\\7e\\00"                             ;; i64, immutable
  "\\42\\80\\00"                          ;; i64.const 0 with unused bits set
  "\\0b"                                ;; end
)`);

// ./test/core/binary-leb128.wast:194
let $22 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\06\\07\\01"                          ;; Global section with 1 entry
  "\\7e\\00"                             ;; i64, immutable
  "\\42\\ff\\7f"                          ;; i64.const -1 with unused bits unset
  "\\0b"                                ;; end
)`);

// ./test/core/binary-leb128.wast:201
let $23 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\06\\0f\\01"                          ;; Global section with 1 entry
  "\\7e\\00"                             ;; i64, immutable
  "\\42\\80\\80\\80\\80\\80\\80\\80\\80\\80\\00"  ;; i64.const 0 with unused bits set
  "\\0b"                                ;; end
)`);

// ./test/core/binary-leb128.wast:208
let $24 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\06\\0f\\01"                          ;; Global section with 1 entry
  "\\7e\\00"                             ;; i64, immutable
  "\\42\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\7f"  ;; i64.const -1 with unused bits unset
  "\\0b"                                ;; end
)`);

// ./test/core/binary-leb128.wast:217
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\08\\01"                          ;; Memory section with 1 entry
    "\\00\\82\\80\\80\\80\\80\\00"              ;; no max, minimum 2 with one byte too many
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:225
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\0a\\01"                          ;; Memory section with 1 entry
    "\\01\\82\\00"                          ;; minimum 2
    "\\82\\80\\80\\80\\80\\00"                 ;; max 2 with one byte too many
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:234
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\03\\01"                          ;; Memory section with 1 entry
    "\\00\\00"                             ;; no max, minimum 0
    "\\0b\\0b\\01"                          ;; Data section with 1 entry
    "\\80\\80\\80\\80\\80\\00"                 ;; Memory index 0 with one byte too many
    "\\41\\00\\0b\\00"                       ;; (i32.const 0) with contents ""
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:245
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\04\\04\\01"                          ;; Table section with 1 entry
    "\\70\\00\\00"                          ;; no max, minimum 0, funcref
    "\\09\\0b\\01"                          ;; Element section with 1 entry
    "\\80\\80\\80\\80\\80\\00"                 ;; Table index 0 with one byte too many
    "\\41\\00\\0b\\00"                       ;; (i32.const 0) with no elements
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:256
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00"                                ;; custom section
    "\\83\\80\\80\\80\\80\\00"                 ;; section size 3 with one byte too many
    "\\01"                                ;; name byte count
    "1"                                  ;; name
    "2"                                  ;; sequence of bytes
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:267
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00"                                ;; custom section
    "\\0A"                                ;; section size
    "\\83\\80\\80\\80\\80\\00"                 ;; name byte count 3 with one byte too many
    "123"                                ;; name
    "4"                                  ;; sequence of bytes
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:278
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

// ./test/core/binary-leb128.wast:290
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

// ./test/core/binary-leb128.wast:302
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

// ./test/core/binary-leb128.wast:317
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

// ./test/core/binary-leb128.wast:332
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

// ./test/core/binary-leb128.wast:347
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

// ./test/core/binary-leb128.wast:359
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

// ./test/core/binary-leb128.wast:375
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

// ./test/core/binary-leb128.wast:391
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

// ./test/core/binary-leb128.wast:404
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

// ./test/core/binary-leb128.wast:423
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

// ./test/core/binary-leb128.wast:442
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

// ./test/core/binary-leb128.wast:461
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

// ./test/core/binary-leb128.wast:482
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0b\\01"                          ;; Global section with 1 entry
    "\\7f\\00"                             ;; i32, immutable
    "\\41\\80\\80\\80\\80\\80\\00"              ;; i32.const 0 with one byte too many
    "\\0b"                                ;; end
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:492
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0b\\01"                          ;; Global section with 1 entry
    "\\7f\\00"                             ;; i32, immutable
    "\\41\\ff\\ff\\ff\\ff\\ff\\7f"              ;; i32.const -1 with one byte too many
    "\\0b"                                ;; end
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:503
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\10\\01"                          ;; Global section with 1 entry
    "\\7e\\00"                             ;; i64, immutable
    "\\42\\80\\80\\80\\80\\80\\80\\80\\80\\80\\80\\00"  ;; i64.const 0 with one byte too many
    "\\0b"                                ;; end
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:513
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\10\\01"                          ;; Global section with 1 entry
    "\\7e\\00"                             ;; i64, immutable
    "\\42\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\7f"  ;; i64.const -1 with one byte too many
    "\\0b"                                ;; end
  )`), `integer representation too long`);

// ./test/core/binary-leb128.wast:525
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\07\\01"                          ;; Memory section with 1 entry
    "\\00\\82\\80\\80\\80\\70"                 ;; no max, minimum 2 with unused bits set
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:533
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\07\\01"                          ;; Memory section with 1 entry
    "\\00\\82\\80\\80\\80\\40"                 ;; no max, minimum 2 with some unused bits set
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:541
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\09\\01"                          ;; Memory section with 1 entry
    "\\01\\82\\00"                          ;; minimum 2
    "\\82\\80\\80\\80\\10"                    ;; max 2 with unused bits set
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:550
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\09\\01"                          ;; Memory section with 1 entry
    "\\01\\82\\00"                          ;; minimum 2
    "\\82\\80\\80\\80\\40"                    ;; max 2 with some unused bits set
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:559
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\03\\01"                          ;; Memory section with 1 entry
    "\\00\\00"                             ;; no max, minimum 0
    "\\0b\\0a\\01"                          ;; Data section with 1 entry
    "\\80\\80\\80\\80\\10"                    ;; Memory index 0 with unused bits set
    "\\41\\00\\0b\\00"                       ;; (i32.const 0) with contents ""
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:570
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\04\\04\\01"                          ;; Table section with 1 entry
    "\\70\\00\\00"                          ;; no max, minimum 0, funcref
    "\\09\\0a\\01"                          ;; Element section with 1 entry
    "\\80\\80\\80\\80\\10"                    ;; Table index 0 with unused bits set
    "\\41\\00\\0b\\00"                       ;; (i32.const 0) with no elements
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:581
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00"                                ;; custom section
    "\\83\\80\\80\\80\\10"                    ;; section size 3 with unused bits set
    "\\01"                                ;; name byte count
    "1"                                  ;; name
    "2"                                  ;; sequence of bytes
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:592
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\00"                                ;; custom section
    "\\09"                                ;; section size
    "\\83\\80\\80\\80\\40"                    ;; name byte count 3 with unused bits set
    "123"                                ;; name
    "4"                                  ;; sequence of bytes
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:603
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

// ./test/core/binary-leb128.wast:615
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

// ./test/core/binary-leb128.wast:627
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

// ./test/core/binary-leb128.wast:642
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

// ./test/core/binary-leb128.wast:657
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

// ./test/core/binary-leb128.wast:672
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

// ./test/core/binary-leb128.wast:685
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

// ./test/core/binary-leb128.wast:701
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

// ./test/core/binary-leb128.wast:717
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

// ./test/core/binary-leb128.wast:730
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

// ./test/core/binary-leb128.wast:749
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

// ./test/core/binary-leb128.wast:768
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

// ./test/core/binary-leb128.wast:786
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

// ./test/core/binary-leb128.wast:805
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

// ./test/core/binary-leb128.wast:824
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

// ./test/core/binary-leb128.wast:843
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
    "\\82\\80\\80\\80\\10"          ;; offset 2 with unused bits set
    "\\0b"                      ;; end
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:862
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

// ./test/core/binary-leb128.wast:884
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0a\\01"                          ;; Global section with 1 entry
    "\\7f\\00"                             ;; i32, immutable
    "\\41\\80\\80\\80\\80\\70"                 ;; i32.const 0 with unused bits set
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:894
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0a\\01"                          ;; Global section with 1 entry
    "\\7f\\00"                             ;; i32, immutable
    "\\41\\ff\\ff\\ff\\ff\\0f"                 ;; i32.const -1 with unused bits unset
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:904
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0a\\01"                          ;; Global section with 1 entry
    "\\7f\\00"                             ;; i32, immutable
    "\\41\\80\\80\\80\\80\\1f"                 ;; i32.const 0 with some unused bits set
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:914
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0a\\01"                          ;; Global section with 1 entry
    "\\7f\\00"                             ;; i32, immutable
    "\\41\\ff\\ff\\ff\\ff\\4f"                 ;; i32.const -1 with some unused bits unset
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:925
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0f\\01"                          ;; Global section with 1 entry
    "\\7e\\00"                             ;; i64, immutable
    "\\42\\80\\80\\80\\80\\80\\80\\80\\80\\80\\7e"  ;; i64.const 0 with unused bits set
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:935
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0f\\01"                          ;; Global section with 1 entry
    "\\7e\\00"                             ;; i64, immutable
    "\\42\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\01"  ;; i64.const -1 with unused bits unset
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:945
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0f\\01"                          ;; Global section with 1 entry
    "\\7e\\00"                             ;; i64, immutable
    "\\42\\80\\80\\80\\80\\80\\80\\80\\80\\80\\02"  ;; i64.const 0 with some unused bits set
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:955
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0f\\01"                          ;; Global section with 1 entry
    "\\7e\\00"                             ;; i64, immutable
    "\\42\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\41"  ;; i64.const -1 with some unused bits unset
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary-leb128.wast:967
let $25 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\01\\04\\01"                          ;; type section
  "\\60\\00\\00"                          ;; empty function type
  "\\03\\02\\01"                          ;; function section
  "\\00"                                ;; function 0, type 0
  "\\0a\\1b\\01\\19"                       ;; code section
  "\\00"                                ;; no locals
  "\\00"                                ;; unreachable
  "\\fc\\80\\00"                          ;; i32_trunc_sat_f32_s with 2 bytes
  "\\00"                                ;; unreachable
  "\\fc\\81\\80\\00"                       ;; i32_trunc_sat_f32_u with 3 bytes
  "\\00"                                ;; unreachable
  "\\fc\\86\\80\\80\\00"                    ;; i64_trunc_sat_f64_s with 4 bytes
  "\\00"                                ;; unreachable
  "\\fc\\87\\80\\80\\80\\00"                 ;; i64_trunc_sat_f64_u with 5 bytes
  "\\00"                                ;; unreachable
  "\\0b"                                ;; end
)`);

// ./test/core/binary-leb128.wast:987
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01"                          ;; type section
    "\\60\\00\\00"                          ;; empty function type
    "\\03\\02\\01"                          ;; function section
    "\\00"                                ;; function 0, type 0
    "\\0a\\0d\\01\\0b"                       ;; code section
    "\\00"                                ;; no locals
    "\\00"                                ;; unreachable
    "\\fc\\87\\80\\80\\80\\80\\00"              ;; i64_trunc_sat_f64_u with 6 bytes
    "\\00"                                ;; unreachable
    "\\0b"                                ;; end
  )`), `integer representation too long`);
