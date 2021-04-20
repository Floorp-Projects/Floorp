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

// ./test/core/binary.wast

// ./test/core/binary.wast:1
let $0 = instantiate(`(module binary "\\00asm\\01\\00\\00\\00")`);

// ./test/core/binary.wast:2
let $1 = instantiate(`(module binary "\\00asm" "\\01\\00\\00\\00")`);

// ./test/core/binary.wast:3
let $2 = instantiate(`(module $$M1 binary "\\00asm\\01\\00\\00\\00")`);
register($2, `M1`);

// ./test/core/binary.wast:4
let $3 = instantiate(`(module $$M2 binary "\\00asm" "\\01\\00\\00\\00")`);
register($3, `M2`);

// ./test/core/binary.wast:6
assert_malformed(() => instantiate(`(module binary "")`), `unexpected end`);

// ./test/core/binary.wast:7
assert_malformed(() => instantiate(`(module binary "\\01")`), `unexpected end`);

// ./test/core/binary.wast:8
assert_malformed(
  () => instantiate(`(module binary "\\00as")`),
  `unexpected end`,
);

// ./test/core/binary.wast:9
assert_malformed(
  () => instantiate(`(module binary "asm\\00")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:10
assert_malformed(
  () => instantiate(`(module binary "msa\\00")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:11
assert_malformed(
  () => instantiate(`(module binary "msa\\00\\01\\00\\00\\00")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:12
assert_malformed(
  () => instantiate(`(module binary "msa\\00\\00\\00\\00\\01")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:13
assert_malformed(
  () => instantiate(`(module binary "asm\\01\\00\\00\\00\\00")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:14
assert_malformed(
  () => instantiate(`(module binary "wasm\\01\\00\\00\\00")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:15
assert_malformed(
  () => instantiate(`(module binary "\\7fasm\\01\\00\\00\\00")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:16
assert_malformed(
  () => instantiate(`(module binary "\\80asm\\01\\00\\00\\00")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:17
assert_malformed(
  () => instantiate(`(module binary "\\82asm\\01\\00\\00\\00")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:18
assert_malformed(
  () => instantiate(`(module binary "\\ffasm\\01\\00\\00\\00")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:21
assert_malformed(
  () => instantiate(`(module binary "\\00\\00\\00\\01msa\\00")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:24
assert_malformed(
  () => instantiate(`(module binary "a\\00ms\\00\\01\\00\\00")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:25
assert_malformed(
  () => instantiate(`(module binary "sm\\00a\\00\\00\\01\\00")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:28
assert_malformed(
  () => instantiate(`(module binary "\\00ASM\\01\\00\\00\\00")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:31
assert_malformed(
  () => instantiate(`(module binary "\\00\\81\\a2\\94\\01\\00\\00\\00")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:34
assert_malformed(
  () => instantiate(`(module binary "\\ef\\bb\\bf\\00asm\\01\\00\\00\\00")`),
  `magic header not detected`,
);

// ./test/core/binary.wast:37
assert_malformed(
  () => instantiate(`(module binary "\\00asm")`),
  `unexpected end`,
);

// ./test/core/binary.wast:38
assert_malformed(
  () => instantiate(`(module binary "\\00asm\\01")`),
  `unexpected end`,
);

// ./test/core/binary.wast:39
assert_malformed(
  () => instantiate(`(module binary "\\00asm\\01\\00\\00")`),
  `unexpected end`,
);

// ./test/core/binary.wast:40
assert_malformed(
  () => instantiate(`(module binary "\\00asm\\00\\00\\00\\00")`),
  `unknown binary version`,
);

// ./test/core/binary.wast:41
assert_malformed(
  () => instantiate(`(module binary "\\00asm\\0d\\00\\00\\00")`),
  `unknown binary version`,
);

// ./test/core/binary.wast:42
assert_malformed(
  () => instantiate(`(module binary "\\00asm\\0e\\00\\00\\00")`),
  `unknown binary version`,
);

// ./test/core/binary.wast:43
assert_malformed(
  () => instantiate(`(module binary "\\00asm\\00\\01\\00\\00")`),
  `unknown binary version`,
);

// ./test/core/binary.wast:44
assert_malformed(
  () => instantiate(`(module binary "\\00asm\\00\\00\\01\\00")`),
  `unknown binary version`,
);

// ./test/core/binary.wast:45
assert_malformed(
  () => instantiate(`(module binary "\\00asm\\00\\00\\00\\01")`),
  `unknown binary version`,
);

// ./test/core/binary.wast:48
assert_malformed(
  () => instantiate(`(module binary "\\00asm" "\\01\\00\\00\\00" "\\0d\\00")`),
  `malformed section id`,
);

// ./test/core/binary.wast:49
assert_malformed(
  () => instantiate(`(module binary "\\00asm" "\\01\\00\\00\\00" "\\7f\\00")`),
  `malformed section id`,
);

// ./test/core/binary.wast:50
assert_malformed(
  () =>
    instantiate(
      `(module binary "\\00asm" "\\01\\00\\00\\00" "\\80\\00\\01\\00")`,
    ),
  `malformed section id`,
);

// ./test/core/binary.wast:51
assert_malformed(
  () =>
    instantiate(
      `(module binary "\\00asm" "\\01\\00\\00\\00" "\\81\\00\\01\\00")`,
    ),
  `malformed section id`,
);

// ./test/core/binary.wast:52
assert_malformed(
  () =>
    instantiate(
      `(module binary "\\00asm" "\\01\\00\\00\\00" "\\ff\\00\\01\\00")`,
    ),
  `malformed section id`,
);

// ./test/core/binary.wast:55
let $4 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\05\\04\\01"                          ;; Memory section with 1 entry
  "\\00\\82\\00"                          ;; no max, minimum 2
)`);

// ./test/core/binary.wast:60
let $5 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\05\\07\\01"                          ;; Memory section with 1 entry
  "\\00\\82\\80\\80\\80\\00"                 ;; no max, minimum 2
)`);

// ./test/core/binary.wast:67
let $6 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\06\\07\\01"                          ;; Global section with 1 entry
  "\\7f\\00"                             ;; i32, immutable
  "\\41\\80\\00"                          ;; i32.const 0
  "\\0b"                                ;; end
)`);

// ./test/core/binary.wast:74
let $7 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\06\\07\\01"                          ;; Global section with 1 entry
  "\\7f\\00"                             ;; i32, immutable
  "\\41\\ff\\7f"                          ;; i32.const -1
  "\\0b"                                ;; end
)`);

// ./test/core/binary.wast:81
let $8 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\06\\0a\\01"                          ;; Global section with 1 entry
  "\\7f\\00"                             ;; i32, immutable
  "\\41\\80\\80\\80\\80\\00"                 ;; i32.const 0
  "\\0b"                                ;; end
)`);

// ./test/core/binary.wast:88
let $9 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\06\\0a\\01"                          ;; Global section with 1 entry
  "\\7f\\00"                             ;; i32, immutable
  "\\41\\ff\\ff\\ff\\ff\\7f"                 ;; i32.const -1
  "\\0b"                                ;; end
)`);

// ./test/core/binary.wast:96
let $10 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\06\\07\\01"                          ;; Global section with 1 entry
  "\\7e\\00"                             ;; i64, immutable
  "\\42\\80\\00"                          ;; i64.const 0 with unused bits set
  "\\0b"                                ;; end
)`);

// ./test/core/binary.wast:103
let $11 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\06\\07\\01"                          ;; Global section with 1 entry
  "\\7e\\00"                             ;; i64, immutable
  "\\42\\ff\\7f"                          ;; i64.const -1 with unused bits unset
  "\\0b"                                ;; end
)`);

// ./test/core/binary.wast:110
let $12 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\06\\0f\\01"                          ;; Global section with 1 entry
  "\\7e\\00"                             ;; i64, immutable
  "\\42\\80\\80\\80\\80\\80\\80\\80\\80\\80\\00"  ;; i64.const 0 with unused bits set
  "\\0b"                                ;; end
)`);

// ./test/core/binary.wast:117
let $13 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\06\\0f\\01"                          ;; Global section with 1 entry
  "\\7e\\00"                             ;; i64, immutable
  "\\42\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\7f"  ;; i64.const -1 with unused bits unset
  "\\0b"                                ;; end
)`);

// ./test/core/binary.wast:125
let $14 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\05\\03\\01"                          ;; Memory section with 1 entry
  "\\00\\00"                             ;; no max, minimum 0
  "\\0b\\06\\01"                          ;; Data section with 1 entry
  "\\00"                                ;; Memory index 0
  "\\41\\00\\0b\\00"                       ;; (i32.const 0) with contents ""
)`);

// ./test/core/binary.wast:134
let $15 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\04\\04\\01"                          ;; Table section with 1 entry
  "\\70\\00\\00"                          ;; no max, minimum 0, funcref
  "\\09\\06\\01"                          ;; Element section with 1 entry
  "\\00"                                ;; Table index 0
  "\\41\\00\\0b\\00"                       ;; (i32.const 0) with no elements
)`);

// ./test/core/binary.wast:144
let $16 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\05\\03\\01"                          ;; Memory section with 1 entry
  "\\00\\00"                             ;; no max, minimum 0
  "\\0b\\07\\01"                          ;; Data section with 1 entry
  "\\80\\00"                             ;; Memory index 0, encoded with 2 bytes
  "\\41\\00\\0b\\00"                       ;; (i32.const 0) with contents ""
)`);

// ./test/core/binary.wast:154
let $17 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\04\\04\\01"                          ;; Table section with 1 entry
  "\\70\\00\\00"                          ;; no max, minimum 0, funcref
  "\\09\\09\\01"                          ;; Element section with 1 entry
  "\\02\\80\\00"                          ;; Table index 0, encoded with 2 bytes
  "\\41\\00\\0b\\00\\00"                    ;; (i32.const 0) with no elements
)`);

// ./test/core/binary.wast:164
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01"                     ;; Type section id
    "\\05"                     ;; Type section length
    "\\01"                     ;; Types vector length
    "\\e0\\7f"                  ;; Malformed functype, -0x20 in signed LEB128 encoding
    "\\00\\00"
  )`), `integer representation too long`);

// ./test/core/binary.wast:177
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\08\\01"                          ;; Memory section with 1 entry
    "\\00\\82\\80\\80\\80\\80\\00"              ;; no max, minimum 2 with one byte too many
  )`), `integer representation too long`);

// ./test/core/binary.wast:187
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0b\\01"                          ;; Global section with 1 entry
    "\\7f\\00"                             ;; i32, immutable
    "\\41\\80\\80\\80\\80\\80\\00"              ;; i32.const 0 with one byte too many
    "\\0b"                                ;; end
  )`), `integer representation too long`);

// ./test/core/binary.wast:197
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0b\\01"                          ;; Global section with 1 entry
    "\\7f\\00"                             ;; i32, immutable
    "\\41\\ff\\ff\\ff\\ff\\ff\\7f"              ;; i32.const -1 with one byte too many
    "\\0b"                                ;; end
  )`), `integer representation too long`);

// ./test/core/binary.wast:208
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\10\\01"                          ;; Global section with 1 entry
    "\\7e\\00"                             ;; i64, immutable
    "\\42\\80\\80\\80\\80\\80\\80\\80\\80\\80\\80\\00"  ;; i64.const 0 with one byte too many
    "\\0b"                                ;; end
  )`), `integer representation too long`);

// ./test/core/binary.wast:218
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\10\\01"                          ;; Global section with 1 entry
    "\\7e\\00"                             ;; i64, immutable
    "\\42\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\7f"  ;; i64.const -1 with one byte too many
    "\\0b"                                ;; end
  )`), `integer representation too long`);

// ./test/core/binary.wast:230
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\07\\01"                          ;; Memory section with 1 entry
    "\\00\\82\\80\\80\\80\\70"                 ;; no max, minimum 2 with unused bits set
  )`), `integer too large`);

// ./test/core/binary.wast:238
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\07\\01"                          ;; Memory section with 1 entry
    "\\00\\82\\80\\80\\80\\40"                 ;; no max, minimum 2 with some unused bits set
  )`), `integer too large`);

// ./test/core/binary.wast:248
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0a\\01"                          ;; Global section with 1 entry
    "\\7f\\00"                             ;; i32, immutable
    "\\41\\80\\80\\80\\80\\70"                 ;; i32.const 0 with unused bits set
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary.wast:258
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0a\\01"                          ;; Global section with 1 entry
    "\\7f\\00"                             ;; i32, immutable
    "\\41\\ff\\ff\\ff\\ff\\0f"                 ;; i32.const -1 with unused bits unset
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary.wast:268
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0a\\01"                          ;; Global section with 1 entry
    "\\7f\\00"                             ;; i32, immutable
    "\\41\\80\\80\\80\\80\\1f"                 ;; i32.const 0 with some unused bits set
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary.wast:278
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0a\\01"                          ;; Global section with 1 entry
    "\\7f\\00"                             ;; i32, immutable
    "\\41\\ff\\ff\\ff\\ff\\4f"                 ;; i32.const -1 with some unused bits unset
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary.wast:289
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0f\\01"                          ;; Global section with 1 entry
    "\\7e\\00"                             ;; i64, immutable
    "\\42\\80\\80\\80\\80\\80\\80\\80\\80\\80\\7e"  ;; i64.const 0 with unused bits set
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary.wast:299
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0f\\01"                          ;; Global section with 1 entry
    "\\7e\\00"                             ;; i64, immutable
    "\\42\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\01"  ;; i64.const -1 with unused bits unset
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary.wast:309
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0f\\01"                          ;; Global section with 1 entry
    "\\7e\\00"                             ;; i64, immutable
    "\\42\\80\\80\\80\\80\\80\\80\\80\\80\\80\\02"  ;; i64.const 0 with some unused bits set
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary.wast:319
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0f\\01"                          ;; Global section with 1 entry
    "\\7e\\00"                             ;; i64, immutable
    "\\42\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\41"  ;; i64.const -1 with some unused bits unset
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary.wast:330
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0f\\01"                          ;; Global section with 1 entry
    "\\7e\\00"                             ;; i64, immutable
    "\\42\\80\\80\\80\\80\\80\\80\\80\\80\\80\\7e"  ;; i64.const 0 with unused bits set
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary.wast:340
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0f\\01"                          ;; Global section with 1 entry
    "\\7e\\00"                             ;; i64, immutable
    "\\42\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\01"  ;; i64.const -1 with unused bits unset
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary.wast:350
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0f\\01"                          ;; Global section with 1 entry
    "\\7e\\00"                             ;; i64, immutable
    "\\42\\80\\80\\80\\80\\80\\80\\80\\80\\80\\02"  ;; i64.const 0 with some unused bits set
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary.wast:360
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0f\\01"                          ;; Global section with 1 entry
    "\\7e\\00"                             ;; i64, immutable
    "\\42\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\41"  ;; i64.const -1 with some unused bits unset
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary.wast:373
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\08\\01"                          ;; Memory section with 1 entry
    "\\00\\82\\80\\80\\80\\80\\00"              ;; no max, minimum 2 with one byte too many
  )`), `integer representation too long`);

// ./test/core/binary.wast:381
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

// ./test/core/binary.wast:400
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

// ./test/core/binary.wast:419
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

// ./test/core/binary.wast:438
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

// ./test/core/binary.wast:459
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0b\\01"                          ;; Global section with 1 entry
    "\\7f\\00"                             ;; i32, immutable
    "\\41\\80\\80\\80\\80\\80\\00"              ;; i32.const 0 with one byte too many
    "\\0b"                                ;; end
  )`), `integer representation too long`);

// ./test/core/binary.wast:469
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0b\\01"                          ;; Global section with 1 entry
    "\\7f\\00"                             ;; i32, immutable
    "\\41\\ff\\ff\\ff\\ff\\ff\\7f"              ;; i32.const -1 with one byte too many
    "\\0b"                                ;; end
  )`), `integer representation too long`);

// ./test/core/binary.wast:480
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\10\\01"                          ;; Global section with 1 entry
    "\\7e\\00"                             ;; i64, immutable
    "\\42\\80\\80\\80\\80\\80\\80\\80\\80\\80\\80\\00"  ;; i64.const 0 with one byte too many
    "\\0b"                                ;; end
  )`), `integer representation too long`);

// ./test/core/binary.wast:490
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\10\\01"                          ;; Global section with 1 entry
    "\\7e\\00"                             ;; i64, immutable
    "\\42\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\7f"  ;; i64.const -1 with one byte too many
    "\\0b"                                ;; end
  )`), `integer representation too long`);

// ./test/core/binary.wast:502
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\07\\01"                          ;; Memory section with 1 entry
    "\\00\\82\\80\\80\\80\\70"                 ;; no max, minimum 2 with unused bits set
  )`), `integer too large`);

// ./test/core/binary.wast:510
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\07\\01"                          ;; Memory section with 1 entry
    "\\00\\82\\80\\80\\80\\40"                 ;; no max, minimum 2 with some unused bits set
  )`), `integer too large`);

// ./test/core/binary.wast:518
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

// ./test/core/binary.wast:537
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

// ./test/core/binary.wast:556
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

// ./test/core/binary.wast:574
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

// ./test/core/binary.wast:593
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

// ./test/core/binary.wast:612
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

// ./test/core/binary.wast:631
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

// ./test/core/binary.wast:650
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

// ./test/core/binary.wast:672
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0a\\01"                          ;; Global section with 1 entry
    "\\7f\\00"                             ;; i32, immutable
    "\\41\\80\\80\\80\\80\\70"                 ;; i32.const 0 with unused bits set
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary.wast:682
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0a\\01"                          ;; Global section with 1 entry
    "\\7f\\00"                             ;; i32, immutable
    "\\41\\ff\\ff\\ff\\ff\\0f"                 ;; i32.const -1 with unused bits unset
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary.wast:692
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0a\\01"                          ;; Global section with 1 entry
    "\\7f\\00"                             ;; i32, immutable
    "\\41\\80\\80\\80\\80\\1f"                 ;; i32.const 0 with some unused bits set
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary.wast:702
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0a\\01"                          ;; Global section with 1 entry
    "\\7f\\00"                             ;; i32, immutable
    "\\41\\ff\\ff\\ff\\ff\\4f"                 ;; i32.const -1 with some unused bits unset
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary.wast:713
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0f\\01"                          ;; Global section with 1 entry
    "\\7e\\00"                             ;; i64, immutable
    "\\42\\80\\80\\80\\80\\80\\80\\80\\80\\80\\7e"  ;; i64.const 0 with unused bits set
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary.wast:723
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0f\\01"                          ;; Global section with 1 entry
    "\\7e\\00"                             ;; i64, immutable
    "\\42\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\01"  ;; i64.const -1 with unused bits unset
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary.wast:733
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0f\\01"                          ;; Global section with 1 entry
    "\\7e\\00"                             ;; i64, immutable
    "\\42\\80\\80\\80\\80\\80\\80\\80\\80\\80\\02"  ;; i64.const 0 with some unused bits set
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary.wast:743
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0f\\01"                          ;; Global section with 1 entry
    "\\7e\\00"                             ;; i64, immutable
    "\\42\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\41"  ;; i64.const -1 with some unused bits unset
    "\\0b"                                ;; end
  )`), `integer too large`);

// ./test/core/binary.wast:755
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\05\\03\\01\\00\\00"          ;; Memory section
    "\\0a\\09\\01"                ;; Code section

    ;; function 0
    "\\07\\00"
    "\\41\\00"                   ;; i32.const 0
    "\\40"                      ;; memory.grow
    "\\01"                      ;; memory.grow reserved byte is not equal to zero!
    "\\1a"                      ;; drop
    "\\0b"                      ;; end
  )`), `zero byte expected`);

// ./test/core/binary.wast:775
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\05\\03\\01\\00\\00"          ;; Memory section
    "\\0a\\0a\\01"                ;; Code section

    ;; function 0
    "\\08\\00"
    "\\41\\00"                   ;; i32.const 0
    "\\40"                      ;; memory.grow
    "\\80\\00"                   ;; memory.grow reserved byte
    "\\1a"                      ;; drop
    "\\0b"                      ;; end
  )`), `zero byte expected`);

// ./test/core/binary.wast:795
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\05\\03\\01\\00\\00"          ;; Memory section
    "\\0a\\0b\\01"                ;; Code section

    ;; function 0
    "\\09\\00"
    "\\41\\00"                   ;; i32.const 0
    "\\40"                      ;; memory.grow
    "\\80\\80\\00"                ;; memory.grow reserved byte
    "\\1a"                      ;; drop
    "\\0b"                      ;; end
  )`), `zero byte expected`);

// ./test/core/binary.wast:814
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\05\\03\\01\\00\\00"          ;; Memory section
    "\\0a\\0c\\01"                ;; Code section

    ;; function 0
    "\\0a\\00"
    "\\41\\00"                   ;; i32.const 0
    "\\40"                      ;; memory.grow
    "\\80\\80\\80\\00"             ;; memory.grow reserved byte
    "\\1a"                      ;; drop
    "\\0b"                      ;; end
  )`), `zero byte expected`);

// ./test/core/binary.wast:833
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\05\\03\\01\\00\\00"          ;; Memory section
    "\\0a\\0d\\01"                ;; Code section

    ;; function 0
    "\\0b\\00"
    "\\41\\00"                   ;; i32.const 0
    "\\40"                      ;; memory.grow
    "\\80\\80\\80\\80\\00"          ;; memory.grow reserved byte
    "\\1a"                      ;; drop
    "\\0b"                      ;; end
  )`), `zero byte expected`);

// ./test/core/binary.wast:853
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\05\\03\\01\\00\\00"          ;; Memory section
    "\\0a\\07\\01"                ;; Code section

    ;; function 0
    "\\05\\00"
    "\\3f"                      ;; memory.size
    "\\01"                      ;; memory.size reserved byte is not equal to zero!
    "\\1a"                      ;; drop
    "\\0b"                      ;; end
  )`), `zero byte expected`);

// ./test/core/binary.wast:872
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\05\\03\\01\\00\\00"          ;; Memory section
    "\\0a\\08\\01"                ;; Code section

    ;; function 0
    "\\06\\00"
    "\\3f"                      ;; memory.size
    "\\80\\00"                   ;; memory.size reserved byte
    "\\1a"                      ;; drop
    "\\0b"                      ;; end
  )`), `zero byte expected`);

// ./test/core/binary.wast:891
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\05\\03\\01\\00\\00"          ;; Memory section
    "\\0a\\09\\01"                ;; Code section

    ;; function 0
    "\\07\\00"
    "\\3f"                      ;; memory.size
    "\\80\\80\\00"                ;; memory.size reserved byte
    "\\1a"                      ;; drop
    "\\0b"                      ;; end
  )`), `zero byte expected`);

// ./test/core/binary.wast:909
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\05\\03\\01\\00\\00"          ;; Memory section
    "\\0a\\0a\\01"                ;; Code section

    ;; function 0
    "\\08\\00"
    "\\3f"                      ;; memory.size
    "\\80\\80\\80\\00"             ;; memory.size reserved byte
    "\\1a"                      ;; drop
    "\\0b"                      ;; end
  )`), `zero byte expected`);

// ./test/core/binary.wast:927
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\05\\03\\01\\00\\00"          ;; Memory section
    "\\0a\\0b\\01"                ;; Code section

    ;; function 0
    "\\09\\00"
    "\\3f"                      ;; memory.size
    "\\80\\80\\80\\80\\00"          ;; memory.size reserved byte
    "\\1a"                      ;; drop
    "\\0b"                      ;; end
  )`), `zero byte expected`);

// ./test/core/binary.wast:946
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\0a\\0c\\01"                ;; Code section

    ;; function 0
    "\\0a\\02"
    "\\80\\80\\80\\80\\10\\7f"       ;; 0x100000000 i32
    "\\02\\7e"                   ;; 0x00000002 i64
    "\\0b"                      ;; end
  )`), `integer too large`);

// ./test/core/binary.wast:963
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\0a\\0c\\01"                ;; Code section

    ;; function 0
    "\\0a\\02"
    "\\80\\80\\80\\80\\10\\7f"       ;; 0x100000000 i32
    "\\02\\7e"                   ;; 0x00000002 i64
    "\\0b"                      ;; end
  )`), `integer too large`);

// ./test/core/binary.wast:980
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\0a\\0c\\01"                ;; Code section

    ;; function 0
    "\\0a\\02"
    "\\ff\\ff\\ff\\ff\\0f\\7f"       ;; 0xFFFFFFFF i32
    "\\02\\7e"                   ;; 0x00000002 i64
    "\\0b"                      ;; end
  )`), `too many locals`);

// ./test/core/binary.wast:996
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\06\\01\\60\\02\\7f\\7f\\00" ;; Type section: (param i32 i32)
    "\\03\\02\\01\\00"             ;; Function section
    "\\0a\\1c\\01"                ;; Code section

    ;; function 0
    "\\1a\\04"
    "\\80\\80\\80\\80\\04\\7f"       ;; 0x40000000 i32
    "\\80\\80\\80\\80\\04\\7e"       ;; 0x40000000 i64
    "\\80\\80\\80\\80\\04\\7d"       ;; 0x40000000 f32
    "\\80\\80\\80\\80\\04\\7c"       ;; 0x40000000 f64
    "\\0b"                      ;; end
  )`), `too many locals`);

// ./test/core/binary.wast:1015
let $18 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\01\\04\\01\\60\\00\\00"     ;; Type section
  "\\03\\02\\01\\00"           ;; Function section
  "\\0a\\0a\\01"              ;; Code section

  ;; function 0
  "\\08\\03"
  "\\00\\7f"                 ;; 0 i32
  "\\00\\7e"                 ;; 0 i64
  "\\02\\7d"                 ;; 2 f32
  "\\0b"                    ;; end
)`);

// ./test/core/binary.wast:1030
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"  ;; Type section
    "\\03\\03\\02\\00\\00"     ;; Function section with 2 functions
  )`), `function and code section have inconsistent lengths`);

// ./test/core/binary.wast:1040
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\0a\\04\\01\\02\\00\\0b"  ;; Code section with 1 empty function
  )`), `function and code section have inconsistent lengths`);

// ./test/core/binary.wast:1049
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"  ;; Type section
    "\\03\\03\\02\\00\\00"     ;; Function section with 2 functions
    "\\0a\\04\\01\\02\\00\\0b"  ;; Code section with 1 empty function
  )`), `function and code section have inconsistent lengths`);

// ./test/core/binary.wast:1060
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"           ;; Type section
    "\\03\\02\\01\\00"                 ;; Function section with 1 function
    "\\0a\\07\\02\\02\\00\\0b\\02\\00\\0b"  ;; Code section with 2 empty functions
  )`), `function and code section have inconsistent lengths`);

// ./test/core/binary.wast:1071
let $19 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\03\\01\\00"  ;; Function section with 0 functions
)`);

// ./test/core/binary.wast:1077
let $20 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\0a\\01\\00"  ;; Code section with 0 functions
)`);

// ./test/core/binary.wast:1083
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\0c\\01\\03"                   ;; Datacount section with value "3"
    "\\0b\\05\\02"                   ;; Data section with two entries
    "\\01\\00"                      ;; Passive data section
    "\\01\\00")`), `data count and data section have inconsistent lengths`);

// ./test/core/binary.wast:1093
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\0c\\01\\01"                   ;; Datacount section with value "1"
    "\\0b\\05\\02"                   ;; Data section with two entries
    "\\01\\00"                      ;; Passive data section
    "\\01\\00")`), `data count and data section have inconsistent lengths`);

// ./test/core/binary.wast:1103
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"

    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\05\\03\\01\\00\\00"          ;; Memory section
    "\\0a\\0e\\01"                ;; Code section

    ;; function 0
    "\\0c\\00"
    "\\41\\00"                   ;; zero args
    "\\41\\00"
    "\\41\\00"
    "\\fc\\08\\00\\00"             ;; memory.init
    "\\0b"

    "\\0b\\03\\01\\01\\00"          ;; Data section
  )`), `data count section required`);

// ./test/core/binary.wast:1125
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"

    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\05\\03\\01\\00\\00"          ;; Memory section
    "\\0a\\07\\01"                ;; Code section

    ;; function 0
    "\\05\\00"
    "\\fc\\09\\00"                ;; data.drop
    "\\0b"

    "\\0b\\03\\01\\01\\00"          ;; Data section
  )`), `data count section required`);

// ./test/core/binary.wast:1144
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"

    "\\01\\04\\01\\60\\00\\00"       ;; Type section

    "\\03\\02\\01\\00"             ;; Function section

    "\\04\\04\\01"                ;; Table section with 1 entry
    "\\70\\00\\00"                ;; no max, minimum 0, funcref

    "\\05\\03\\01\\00\\00"          ;; Memory section

    "\\09\\07\\01"                ;; Element section with one segment
    "\\05\\70"                   ;; Passive, funcref
    "\\01"                      ;; 1 element
    "\\d3\\00\\0b"                ;; bad opcode, index 0, end

    "\\0a\\04\\01"                ;; Code section

    ;; function 0
    "\\02\\00"
    "\\0b")`), `illegal opcode`);

// ./test/core/binary.wast:1170
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"

    "\\01\\04\\01\\60\\00\\00"       ;; Type section

    "\\03\\02\\01\\00"             ;; Function section

    "\\04\\04\\01"                ;; Table section with 1 entry
    "\\70\\00\\00"                ;; no max, minimum 0, funcref

    "\\05\\03\\01\\00\\00"          ;; Memory section

    "\\09\\07\\01"                ;; Element section with one segment
    "\\05\\7f"                   ;; Passive, i32
    "\\01"                      ;; 1 element
    "\\d2\\00\\0b"                ;; ref.func, index 0, end

    "\\0a\\04\\01"                ;; Code section

    ;; function 0
    "\\02\\00"
    "\\0b")`), `malformed reference type`);

// ./test/core/binary.wast:1196
let $21 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"

  "\\01\\04\\01\\60\\00\\00"       ;; Type section

  "\\03\\02\\01\\00"             ;; Function section

  "\\04\\04\\01"                ;; Table section with 1 entry
  "\\70\\00\\00"                ;; no max, minimum 0, funcref

  "\\05\\03\\01\\00\\00"          ;; Memory section

  "\\09\\07\\01"                ;; Element section with one segment
  "\\05\\70"                   ;; Passive, funcref
  "\\01"                      ;; 1 element
  "\\d2\\00\\0b"                ;; ref.func, index 0, end

  "\\0a\\04\\01"                ;; Code section

  ;; function 0
  "\\02\\00"
  "\\0b")`);

// ./test/core/binary.wast:1220
let $22 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"

  "\\01\\04\\01\\60\\00\\00"       ;; Type section

  "\\03\\02\\01\\00"             ;; Function section

  "\\04\\04\\01"                ;; Table section with 1 entry
  "\\70\\00\\00"                ;; no max, minimum 0, funcref

  "\\05\\03\\01\\00\\00"          ;; Memory section

  "\\09\\07\\01"                ;; Element section with one segment
  "\\05\\70"                   ;; Passive, funcref
  "\\01"                      ;; 1 element
  "\\d0\\70\\0b"                ;; ref.null, end

  "\\0a\\04\\01"                ;; Code section

  ;; function 0
  "\\02\\00"
  "\\0b")`);

// ./test/core/binary.wast:1245
let $23 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\01\\01\\00"                               ;; type count can be zero
)`);

// ./test/core/binary.wast:1251
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\07\\02"                             ;; type section with inconsistent count (2 declared, 1 given)
    "\\60\\00\\00"                             ;; 1st type
    ;; "\\60\\00\\00"                          ;; 2nd type (missed)
  )`), `unexpected end of section or function`);

// ./test/core/binary.wast:1262
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\07\\01"                             ;; type section with inconsistent count (1 declared, 2 given)
    "\\60\\00\\00"                             ;; 1st type
    "\\60\\00\\00"                             ;; 2nd type (redundant)
  )`), `section size mismatch`);

// ./test/core/binary.wast:1273
let $24 = instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\05\\01"                             ;; type section
    "\\60\\01\\7f\\00"                          ;; type 0
    "\\02\\01\\00"                             ;; import count can be zero
)`);

// ./test/core/binary.wast:1281
assert_malformed(() =>
  instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\02\\04\\01"                           ;; import section with single entry
      "\\00"                                 ;; string length 0
      "\\00"                                 ;; string length 0
      "\\04"                                 ;; malformed import kind
  )`), `malformed import kind`);

// ./test/core/binary.wast:1291
assert_malformed(() =>
  instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\02\\05\\01"                           ;; import section with single entry
      "\\00"                                 ;; string length 0
      "\\00"                                 ;; string length 0
      "\\04"                                 ;; malformed import kind
      "\\00"                                 ;; dummy byte
  )`), `malformed import kind`);

// ./test/core/binary.wast:1302
assert_malformed(() =>
  instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\02\\04\\01"                           ;; import section with single entry
      "\\00"                                 ;; string length 0
      "\\00"                                 ;; string length 0
      "\\05"                                 ;; malformed import kind
  )`), `malformed import kind`);

// ./test/core/binary.wast:1312
assert_malformed(() =>
  instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\02\\05\\01"                           ;; import section with single entry
      "\\00"                                 ;; string length 0
      "\\00"                                 ;; string length 0
      "\\05"                                 ;; malformed import kind
      "\\00"                                 ;; dummy byte
  )`), `malformed import kind`);

// ./test/core/binary.wast:1323
assert_malformed(() =>
  instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\02\\04\\01"                           ;; import section with single entry
      "\\00"                                 ;; string length 0
      "\\00"                                 ;; string length 0
      "\\80"                                 ;; malformed import kind
  )`), `malformed import kind`);

// ./test/core/binary.wast:1333
assert_malformed(() =>
  instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\02\\05\\01"                           ;; import section with single entry
      "\\00"                                 ;; string length 0
      "\\00"                                 ;; string length 0
      "\\80"                                 ;; malformed import kind
      "\\00"                                 ;; dummy byte
  )`), `malformed import kind`);

// ./test/core/binary.wast:1346
assert_malformed(() =>
  instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\01\\05\\01"                           ;; type section
      "\\60\\01\\7f\\00"                        ;; type 0
      "\\02\\16\\02"                           ;; import section with inconsistent count (2 declared, 1 given)
      ;; 1st import
      "\\08"                                 ;; string length
      "\\73\\70\\65\\63\\74\\65\\73\\74"            ;; spectest
      "\\09"                                 ;; string length
      "\\70\\72\\69\\6e\\74\\5f\\69\\33\\32"         ;; print_i32
      "\\00\\00"                              ;; import kind, import signature index
      ;; 2nd import
      ;; (missed)
  )`), `unexpected end of section or function`);

// ./test/core/binary.wast:1365
assert_malformed(() =>
  instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\01\\09\\02"                           ;; type section
      "\\60\\01\\7f\\00"                        ;; type 0
      "\\60\\01\\7d\\00"                        ;; type 1
      "\\02\\2b\\01"                           ;; import section with inconsistent count (1 declared, 2 given)
      ;; 1st import
      "\\08"                                 ;; string length
      "\\73\\70\\65\\63\\74\\65\\73\\74"            ;; spectest
      "\\09"                                 ;; string length
      "\\70\\72\\69\\6e\\74\\5f\\69\\33\\32"         ;; print_i32
      "\\00\\00"                              ;; import kind, import signature index
      ;; 2nd import
      ;; (redundant)
      "\\08"                                 ;; string length
      "\\73\\70\\65\\63\\74\\65\\73\\74"            ;; spectest
      "\\09"                                 ;; string length
      "\\70\\72\\69\\6e\\74\\5f\\66\\33\\32"         ;; print_f32
      "\\00\\01"                              ;; import kind, import signature index
  )`), `section size mismatch`);

// ./test/core/binary.wast:1390
let $25 = instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\04\\01\\00"                             ;; table count can be zero
)`);

// ./test/core/binary.wast:1396
assert_malformed(() =>
  instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\04\\01\\01"                           ;; table section with inconsistent count (1 declared, 0 given)
      ;; "\\70\\01\\00\\00"                     ;; table entity
  )`), `unexpected end of section or function`);

// ./test/core/binary.wast:1406
assert_malformed(() =>
  instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\05\\03\\01"                           ;; table section with one entry
      "\\70"                                 ;; anyfunc
      "\\02"                                 ;; malformed table limits flag
  )`), `integer too large`);

// ./test/core/binary.wast:1415
assert_malformed(() =>
  instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\05\\04\\01"                           ;; table section with one entry
      "\\70"                                 ;; anyfunc
      "\\02"                                 ;; malformed table limits flag
      "\\00"                                 ;; dummy byte
  )`), `integer too large`);

// ./test/core/binary.wast:1425
assert_malformed(() =>
  instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\05\\06\\01"                           ;; table section with one entry
      "\\70"                                 ;; anyfunc
      "\\81\\00"                              ;; malformed table limits flag as LEB128
      "\\00\\00"                              ;; dummy bytes
  )`), `integer too large`);

// ./test/core/binary.wast:1437
let $26 = instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\01\\00"                             ;; memory count can be zero
)`);

// ./test/core/binary.wast:1443
assert_malformed(() =>
  instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\05\\01\\01"                           ;; memory section with inconsistent count (1 declared, 0 given)
      ;; "\\00\\00"                           ;; memory 0 (missed)
  )`), `unexpected end of section or function`);

// ./test/core/binary.wast:1453
assert_malformed(() =>
  instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\05\\02\\01"                           ;; memory section with one entry
      "\\02"                                 ;; malformed memory limits flag
  )`), `integer too large`);

// ./test/core/binary.wast:1461
assert_malformed(() =>
  instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\05\\03\\01"                           ;; memory section with one entry
      "\\02"                                 ;; malformed memory limits flag
      "\\00"                                 ;; dummy byte
  )`), `integer too large`);

// ./test/core/binary.wast:1470
assert_malformed(() =>
  instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\05\\05\\01"                           ;; memory section with one entry
      "\\81\\00"                              ;; malformed memory limits flag as LEB128
      "\\00\\00"                              ;; dummy bytes
  )`), `integer representation too long`);

// ./test/core/binary.wast:1479
assert_malformed(() =>
  instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\05\\05\\01"                           ;; memory section with one entry
      "\\81\\01"                              ;; malformed memory limits flag as LEB128
      "\\00\\00"                              ;; dummy bytes
  )`), `integer representation too long`);

// ./test/core/binary.wast:1490
let $27 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\06\\01\\00"                               ;; global count can be zero
)`);

// ./test/core/binary.wast:1496
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\06\\02"                             ;; global section with inconsistent count (2 declared, 1 given)
    "\\7f\\00\\41\\00\\0b"                       ;; global 0
    ;; "\\7f\\00\\41\\00\\0b"                    ;; global 1 (missed)
  )`), `unexpected end of section or function`);

// ./test/core/binary.wast:1507
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0b\\01"                             ;; global section with inconsistent count (1 declared, 2 given)
    "\\7f\\00\\41\\00\\0b"                       ;; global 0
    "\\7f\\00\\41\\00\\0b"                       ;; global 1 (redundant)
  )`), `section size mismatch`);

// ./test/core/binary.wast:1518
let $28 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\01\\04\\01"                               ;; type section
  "\\60\\00\\00"                               ;; type 0
  "\\03\\03\\02\\00\\00"                         ;; func section
  "\\07\\01\\00"                               ;; export count can be zero
  "\\0a\\07\\02"                               ;; code section
  "\\02\\00\\0b"                               ;; function body 0
  "\\02\\00\\0b"                               ;; function body 1
)`);

// ./test/core/binary.wast:1530
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01"                             ;; type section
    "\\60\\00\\00"                             ;; type 0
    "\\03\\03\\02\\00\\00"                       ;; func section
    "\\07\\06\\02"                             ;; export section with inconsistent count (2 declared, 1 given)
    "\\02"                                   ;; export 0
    "\\66\\31"                                ;; export name
    "\\00\\00"                                ;; export kind, export func index
    ;; "\\02"                                ;; export 1 (missed)
    ;; "\\66\\32"                             ;; export name
    ;; "\\00\\01"                             ;; export kind, export func index
    "\\0a\\07\\02"                             ;; code section
    "\\02\\00\\0b"                             ;; function body 0
    "\\02\\00\\0b"                             ;; function body 1
  )`), `unexpected end of section or function`);

// ./test/core/binary.wast:1551
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01"                             ;; type section
    "\\60\\00\\00"                             ;; type 0
    "\\03\\03\\02\\00\\00"                       ;; func section
    "\\07\\0b\\01"                             ;; export section with inconsistent count (1 declared, 2 given)
    "\\02"                                   ;; export 0
    "\\66\\31"                                ;; export name
    "\\00\\00"                                ;; export kind, export func index
    "\\02"                                   ;; export 1 (redundant)
    "\\66\\32"                                ;; export name
    "\\00\\01"                                ;; export kind, export func index
    "\\0a\\07\\02"                             ;; code section
    "\\02\\00\\0b"                             ;; function body 0
    "\\02\\00\\0b"                             ;; function body 1
  )`), `section size mismatch`);

// ./test/core/binary.wast:1572
let $29 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\01\\04\\01"                               ;; type section
  "\\60\\00\\00"                               ;; type 0
  "\\03\\02\\01\\00"                            ;; func section
  "\\04\\04\\01"                               ;; table section
  "\\70\\00\\01"                               ;; table 0
  "\\09\\01\\00"                               ;; elem segment count can be zero
  "\\0a\\04\\01"                               ;; code section
  "\\02\\00\\0b"                               ;; function body
)`);

// ./test/core/binary.wast:1585
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01"                             ;; type section
    "\\60\\00\\00"                             ;; type 0
    "\\03\\02\\01\\00"                          ;; func section
    "\\04\\04\\01"                             ;; table section
    "\\70\\00\\01"                             ;; table 0
    "\\09\\07\\02"                             ;; elem with inconsistent segment count (2 declared, 1 given)
    "\\00\\41\\00\\0b\\01\\00"                    ;; elem 0
    ;; "\\00\\41\\00\\0b\\01\\00"                 ;; elem 1 (missed)
  )`), `unexpected end`);

// ./test/core/binary.wast:1601
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01"                             ;; type section
    "\\60\\00\\00"                             ;; type 0
    "\\03\\02\\01\\00"                          ;; func section
    "\\04\\04\\01"                             ;; table section
    "\\70\\00\\01"                             ;; table 0
    "\\09\\07\\02"                             ;; elem with inconsistent segment count (2 declared, 1 given)
    "\\00\\41\\00\\0b\\01\\00"                    ;; elem 0
    "\\00\\41\\00"                             ;; elem 1 (partial)
    ;; "\\0b\\01\\00"                          ;; elem 1 (missing part)
  )`), `unexpected end`);

// ./test/core/binary.wast:1618
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01"                             ;; type section
    "\\60\\00\\00"                             ;; type 0
    "\\03\\02\\01\\00"                          ;; func section
    "\\04\\04\\01"                             ;; table section
    "\\70\\00\\01"                             ;; table 0
    "\\09\\0d\\01"                             ;; elem with inconsistent segment count (1 declared, 2 given)
    "\\00\\41\\00\\0b\\01\\00"                    ;; elem 0
    "\\00\\41\\00\\0b\\01\\00"                    ;; elem 1 (redundant)
    "\\0a\\04\\01"                             ;; code section
    "\\02\\00\\0b"                             ;; function body
  )`), `section size mismatch`);

// ./test/core/binary.wast:1636
let $30 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\05\\03\\01"                               ;; memory section
  "\\00\\01"                                  ;; memory 0
  "\\0b\\01\\00"                               ;; data segment count can be zero
)`);

// ./test/core/binary.wast:1644
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\03\\01"                             ;; memory section
    "\\00\\01"                                ;; memory 0
    "\\0b\\07\\02"                             ;; data with inconsistent segment count (2 declared, 1 given)
    "\\00\\41\\00\\0b\\01\\61"                    ;; data 0
    ;; "\\00\\41\\01\\0b\\01\\62"                 ;; data 1 (missed)
  )`), `unexpected end of section or function`);

// ./test/core/binary.wast:1657
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\03\\01"                             ;; memory section
    "\\00\\01"                                ;; memory 0
    "\\0b\\0d\\01"                             ;; data with inconsistent segment count (1 declared, 2 given)
    "\\00\\41\\00\\0b\\01\\61"                    ;; data 0
    "\\00\\41\\01\\0b\\01\\62"                    ;; data 1 (redundant)
  )`), `section size mismatch`);

// ./test/core/binary.wast:1670
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\03\\01"                             ;; memory section
    "\\00\\01"                                ;; memory 0
    "\\0b\\0c\\01"                             ;; data section
    "\\00\\41\\03\\0b"                          ;; data segment 0
    "\\07"                                   ;; data segment size with inconsistent lengths (7 declared, 6 given)
    "\\61\\62\\63\\64\\65\\66"                    ;; 6 bytes given
  )`), `unexpected end of section or function`);

// ./test/core/binary.wast:1684
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\03\\01"                             ;; memory section
    "\\00\\01"                                ;; memory 0
    "\\0b\\0c\\01"                             ;; data section
    "\\00\\41\\00\\0b"                          ;; data segment 0
    "\\05"                                   ;; data segment size with inconsistent lengths (5 declared, 6 given)
    "\\61\\62\\63\\64\\65\\66"                    ;; 6 bytes given
  )`), `section size mismatch`);

// ./test/core/binary.wast:1698
let $31 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\01\\04\\01"                               ;; type section
  "\\60\\00\\00"                               ;; type 0
  "\\03\\02\\01\\00"                            ;; func section
  "\\0a\\11\\01"                               ;; code section
  "\\0f\\00"                                  ;; func 0
  "\\02\\40"                                  ;; block 0
  "\\41\\01"                                  ;; condition of if 0
  "\\04\\40"                                  ;; if 0
  "\\41\\01"                                  ;; index of br_table element
  "\\0e\\00"                                  ;; br_table target count can be zero
  "\\02"                                     ;; break depth for default
  "\\0b\\0b\\0b"                               ;; end
)`);

// ./test/core/binary.wast:1715
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01"                             ;; type section
    "\\60\\00\\00"                             ;; type 0
    "\\03\\02\\01\\00"                          ;; func section
    "\\0a\\12\\01"                             ;; code section
    "\\11\\00"                                ;; func 0
    "\\02\\40"                                ;; block 0
    "\\41\\01"                                ;; condition of if 0
    "\\04\\40"                                ;; if 0
    "\\41\\01"                                ;; index of br_table element
    "\\0e\\01"                                ;; br_table with inconsistent target count (1 declared, 2 given)
    "\\00"                                   ;; break depth 0
    "\\01"                                   ;; break depth 1
    "\\02"                                   ;; break depth for default
    "\\0b\\0b\\0b"                             ;; end
  )`), `unexpected end`);

// ./test/core/binary.wast:1737
let $32 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\01\\04\\01\\60\\00\\00"       ;; Type section
  "\\03\\02\\01\\00"             ;; Function section
  "\\08\\01\\00"                ;; Start section: function 0

  "\\0a\\04\\01"                ;; Code section
  ;; function 0
  "\\02\\00"
  "\\0b"                      ;; end
)`);

// ./test/core/binary.wast:1750
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section
    "\\03\\02\\01\\00"             ;; Function section
    "\\08\\01\\00"                ;; Start section: function 0
    "\\08\\01\\00"                ;; Start section: function 0

    "\\0a\\04\\01"                ;; Code section
    ;; function 0
    "\\02\\00"
    "\\0b"                      ;; end
  )`), `junk after last section`);
