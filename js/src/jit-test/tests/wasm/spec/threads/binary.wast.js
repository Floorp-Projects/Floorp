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

// ./test/core/binary.wast:49
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"      ;; Type section
    "\\03\\02\\01\\00"            ;; Function section
    "\\04\\04\\01\\70\\00\\00"      ;; Table section
    "\\0a\\09\\01"               ;; Code section

    ;; function 0
    "\\07\\00"
    "\\41\\00"                   ;; i32.const 0
    "\\11\\00"                   ;; call_indirect (type 0)
    "\\01"                      ;; call_indirect reserved byte is not equal to zero!
    "\\0b"                      ;; end
  )`), `zero flag expected`);

// ./test/core/binary.wast:68
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"      ;; Type section
    "\\03\\02\\01\\00"            ;; Function section
    "\\04\\04\\01\\70\\00\\00"      ;; Table section
    "\\0a\\0a\\01"               ;; Code section

    ;; function 0
    "\\07\\00"
    "\\41\\00"                   ;; i32.const 0
    "\\11\\00"                   ;; call_indirect (type 0)
    "\\80\\00"                   ;; call_indirect reserved byte
    "\\0b"                      ;; end
  )`), `zero flag expected`);

// ./test/core/binary.wast:87
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"      ;; Type section
    "\\03\\02\\01\\00"            ;; Function section
    "\\04\\04\\01\\70\\00\\00"      ;; Table section
    "\\0a\\0b\\01"               ;; Code section

    ;; function 0
    "\\08\\00"
    "\\41\\00"                   ;; i32.const 0
    "\\11\\00"                   ;; call_indirect (type 0)
    "\\80\\80\\00"                ;; call_indirect reserved byte
    "\\0b"                      ;; end
  )`), `zero flag expected`);

// ./test/core/binary.wast:105
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"      ;; Type section
    "\\03\\02\\01\\00"            ;; Function section
    "\\04\\04\\01\\70\\00\\00"      ;; Table section
    "\\0a\\0c\\01"               ;; Code section

    ;; function 0
    "\\09\\00"
    "\\41\\00"                   ;; i32.const 0
    "\\11\\00"                   ;; call_indirect (type 0)
    "\\80\\80\\80\\00"             ;; call_indirect reserved byte
    "\\0b"                      ;; end
  )`), `zero flag expected`);

// ./test/core/binary.wast:123
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"      ;; Type section
    "\\03\\02\\01\\00"            ;; Function section
    "\\04\\04\\01\\70\\00\\00"      ;; Table section
    "\\0a\\0d\\01"               ;; Code section

    ;; function 0
    "\\0a\\00"
    "\\41\\00"                   ;; i32.const 0
    "\\11\\00"                   ;; call_indirect (type 0)
    "\\80\\80\\80\\80\\00"          ;; call_indirect reserved byte
    "\\0b"                      ;; end
  )`), `zero flag expected`);

// ./test/core/binary.wast:142
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
  )`), `zero flag expected`);

// ./test/core/binary.wast:162
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
  )`), `zero flag expected`);

// ./test/core/binary.wast:182
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
  )`), `zero flag expected`);

// ./test/core/binary.wast:201
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
  )`), `zero flag expected`);

// ./test/core/binary.wast:220
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
  )`), `zero flag expected`);

// ./test/core/binary.wast:240
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
  )`), `zero flag expected`);

// ./test/core/binary.wast:259
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
  )`), `zero flag expected`);

// ./test/core/binary.wast:278
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
  )`), `zero flag expected`);

// ./test/core/binary.wast:296
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
  )`), `zero flag expected`);

// ./test/core/binary.wast:314
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
  )`), `zero flag expected`);

// ./test/core/binary.wast:333
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

// ./test/core/binary.wast:350
let $4 = instantiate(`(module binary
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

// ./test/core/binary.wast:365
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"  ;; Type section
    "\\03\\03\\02\\00\\00"     ;; Function section with 2 functions
  )`), `function and code section have inconsistent lengths`);

// ./test/core/binary.wast:375
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\0a\\04\\01\\02\\00\\0b"  ;; Code section with 1 empty function
  )`), `function and code section have inconsistent lengths`);

// ./test/core/binary.wast:384
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"  ;; Type section
    "\\03\\03\\02\\00\\00"     ;; Function section with 2 functions
    "\\0a\\04\\01\\02\\00\\0b"  ;; Code section with 1 empty function
  )`), `function and code section have inconsistent lengths`);

// ./test/core/binary.wast:395
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"           ;; Type section
    "\\03\\02\\01\\00"                 ;; Function section with 1 function
    "\\0a\\07\\02\\02\\00\\0b\\02\\00\\0b"  ;; Code section with 2 empty functions
  )`), `function and code section have inconsistent lengths`);

// ./test/core/binary.wast:406
let $5 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\03\\01\\00"  ;; Function section with 0 functions
)`);

// ./test/core/binary.wast:412
let $6 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\0a\\01\\00"  ;; Code section with 0 functions
)`);

// ./test/core/binary.wast:418
let $7 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\01\\01\\00"                               ;; type count can be zero
)`);

// ./test/core/binary.wast:424
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\07\\02"                             ;; type section with inconsistent count (2 declared, 1 given)
    "\\60\\00\\00"                             ;; 1st type
    ;; "\\60\\00\\00"                          ;; 2nd type (missed)
  )`), `unexpected end of section or function`);

// ./test/core/binary.wast:435
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\07\\01"                             ;; type section with inconsistent count (1 declared, 2 given)
    "\\60\\00\\00"                             ;; 1st type
    "\\60\\00\\00"                             ;; 2nd type (redundant)
  )`), `section size mismatch`);

// ./test/core/binary.wast:446
let $8 = instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\05\\01"                             ;; type section
    "\\60\\01\\7f\\00"                          ;; type 0
    "\\02\\01\\00"                             ;; import count can be zero
)`);

// ./test/core/binary.wast:454
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

// ./test/core/binary.wast:473
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

// ./test/core/binary.wast:498
let $9 = instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\04\\01\\00"                             ;; table count can be zero
)`);

// ./test/core/binary.wast:504
assert_malformed(() =>
  instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\04\\01\\01"                           ;; table section with inconsistent count (1 declared, 0 given)
      ;; "\\70\\01\\00\\00"                     ;; table entity
  )`), `unexpected end of section or function`);

// ./test/core/binary.wast:514
let $10 = instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\01\\00"                             ;; memory count can be zero
)`);

// ./test/core/binary.wast:520
assert_malformed(() =>
  instantiate(`(module binary
      "\\00asm" "\\01\\00\\00\\00"
      "\\05\\01\\01"                           ;; memory section with inconsistent count (1 declared, 0 given)
      ;; "\\00\\00"                           ;; memory 0 (missed)
  )`), `unexpected end of section or function`);

// ./test/core/binary.wast:530
let $11 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\06\\01\\00"                               ;; global count can be zero
)`);

// ./test/core/binary.wast:536
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\06\\02"                             ;; global section with inconsistent count (2 declared, 1 given)
    "\\7f\\00\\41\\00\\0b"                       ;; global 0
    ;; "\\7f\\00\\41\\00\\0b"                    ;; global 1 (missed)
  )`), `unexpected end of section or function`);

// ./test/core/binary.wast:547
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\0b\\01"                             ;; global section with inconsistent count (1 declared, 2 given)
    "\\7f\\00\\41\\00\\0b"                       ;; global 0
    "\\7f\\00\\41\\00\\0b"                       ;; global 1 (redundant)
  )`), `section size mismatch`);

// ./test/core/binary.wast:558
let $12 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\01\\04\\01"                               ;; type section
  "\\60\\00\\00"                               ;; type 0
  "\\03\\03\\02\\00\\00"                         ;; func section
  "\\07\\01\\00"                               ;; export count can be zero
  "\\0a\\07\\02"                               ;; code section
  "\\02\\00\\0b"                               ;; function body 0
  "\\02\\00\\0b"                               ;; function body 1
)`);

// ./test/core/binary.wast:570
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

// ./test/core/binary.wast:591
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

// ./test/core/binary.wast:612
let $13 = instantiate(`(module binary
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

// ./test/core/binary.wast:625
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
    "\\0a\\04\\01"                             ;; code section
    "\\02\\00\\0b"                             ;; function body
  )`), `invalid value type`);

// ./test/core/binary.wast:643
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

// ./test/core/binary.wast:661
let $14 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\05\\03\\01"                               ;; memory section
  "\\00\\01"                                  ;; memory 0
  "\\0b\\01\\00"                               ;; data segment count can be zero
)`);

// ./test/core/binary.wast:669
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\03\\01"                             ;; memory section
    "\\00\\01"                                ;; memory 0
    "\\0b\\07\\02"                             ;; data with inconsistent segment count (2 declared, 1 given)
    "\\00\\41\\00\\0b\\01\\61"                    ;; data 0
    ;; "\\00\\41\\01\\0b\\01\\62"                 ;; data 1 (missed)
  )`), `unexpected end of section or function`);

// ./test/core/binary.wast:682
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\03\\01"                             ;; memory section
    "\\00\\01"                                ;; memory 0
    "\\0b\\0d\\01"                             ;; data with inconsistent segment count (1 declared, 2 given)
    "\\00\\41\\00\\0b\\01\\61"                    ;; data 0
    "\\00\\41\\01\\0b\\01\\62"                    ;; data 1 (redundant)
  )`), `section size mismatch`);

// ./test/core/binary.wast:695
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

// ./test/core/binary.wast:709
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

// ./test/core/binary.wast:723
let $15 = instantiate(`(module binary
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

// ./test/core/binary.wast:740
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01"                             ;; type section
    "\\60\\00\\00"                             ;; type 0
    "\\03\\02\\01\\00"                          ;; func section
    "\\0a\\12\\01"                             ;; code section
    "\\10\\00"                                ;; func 0
    "\\02\\40"                                ;; block 0
    "\\41\\01"                                ;; condition of if 0
    "\\04\\40"                                ;; if 0
    "\\41\\01"                                ;; index of br_table element
    "\\0e\\02"                                ;; br_table with inconsistent target count (2 declared, 1 given)
    "\\00"                                   ;; break depth 0
    ;; "\\01"                                ;; break depth 1 (missed)
    "\\02"                                   ;; break depth for default
    "\\0b\\0b\\0b"                             ;; end
  )`), `unexpected end of section or function`);

// ./test/core/binary.wast:762
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
  )`), `invalid value type`);

// ./test/core/binary.wast:784
let $16 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"
  "\\01\\04\\01\\60\\00\\00"       ;; Type section
  "\\03\\02\\01\\00"             ;; Function section
  "\\08\\01\\00"                ;; Start section: function 0

  "\\0a\\04\\01"                ;; Code section
  ;; function 0
  "\\02\\00"
  "\\0b"                      ;; end
)`);

// ./test/core/binary.wast:797
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
