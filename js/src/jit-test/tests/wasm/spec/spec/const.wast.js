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

// ./test/core/const.wast

// ./test/core/const.wast:5
let $0 = instantiate(`(module (func (i32.const 0_123_456_789) drop))`);

// ./test/core/const.wast:6
let $1 = instantiate(`(module (func (i32.const 0x0_9acf_fBDF) drop))`);

// ./test/core/const.wast:7
assert_malformed(() => instantiate(`(func (i32.const) drop) `), `unexpected token`);

// ./test/core/const.wast:11
assert_malformed(() => instantiate(`(func (i32.const 0x) drop) `), `unknown operator`);

// ./test/core/const.wast:15
assert_malformed(() => instantiate(`(func (i32.const 1x) drop) `), `unknown operator`);

// ./test/core/const.wast:19
assert_malformed(() => instantiate(`(func (i32.const 0xg) drop) `), `unknown operator`);

// ./test/core/const.wast:24
let $2 = instantiate(`(module (func (i64.const 0_123_456_789) drop))`);

// ./test/core/const.wast:25
let $3 = instantiate(`(module (func (i64.const 0x0125_6789_ADEF_bcef) drop))`);

// ./test/core/const.wast:26
assert_malformed(() => instantiate(`(func (i64.const) drop) `), `unexpected token`);

// ./test/core/const.wast:30
assert_malformed(() => instantiate(`(func (i64.const 0x) drop) `), `unknown operator`);

// ./test/core/const.wast:34
assert_malformed(() => instantiate(`(func (i64.const 1x) drop) `), `unknown operator`);

// ./test/core/const.wast:38
assert_malformed(() => instantiate(`(func (i64.const 0xg) drop) `), `unknown operator`);

// ./test/core/const.wast:43
let $4 = instantiate(`(module (func (f32.const 0123456789) drop))`);

// ./test/core/const.wast:44
let $5 = instantiate(`(module (func (f32.const 0123456789e019) drop))`);

// ./test/core/const.wast:45
let $6 = instantiate(`(module (func (f32.const 0123456789e+019) drop))`);

// ./test/core/const.wast:46
let $7 = instantiate(`(module (func (f32.const 0123456789e-019) drop))`);

// ./test/core/const.wast:47
let $8 = instantiate(`(module (func (f32.const 0123456789.) drop))`);

// ./test/core/const.wast:48
let $9 = instantiate(`(module (func (f32.const 0123456789.e019) drop))`);

// ./test/core/const.wast:49
let $10 = instantiate(`(module (func (f32.const 0123456789.e+019) drop))`);

// ./test/core/const.wast:50
let $11 = instantiate(`(module (func (f32.const 0123456789.e-019) drop))`);

// ./test/core/const.wast:51
let $12 = instantiate(`(module (func (f32.const 0123456789.0123456789) drop))`);

// ./test/core/const.wast:52
let $13 = instantiate(`(module (func (f32.const 0123456789.0123456789e019) drop))`);

// ./test/core/const.wast:53
let $14 = instantiate(`(module (func (f32.const 0123456789.0123456789e+019) drop))`);

// ./test/core/const.wast:54
let $15 = instantiate(`(module (func (f32.const 0123456789.0123456789e-019) drop))`);

// ./test/core/const.wast:55
let $16 = instantiate(`(module (func (f32.const 0x0123456789ABCDEF) drop))`);

// ./test/core/const.wast:56
let $17 = instantiate(`(module (func (f32.const 0x0123456789ABCDEFp019) drop))`);

// ./test/core/const.wast:57
let $18 = instantiate(`(module (func (f32.const 0x0123456789ABCDEFp+019) drop))`);

// ./test/core/const.wast:58
let $19 = instantiate(`(module (func (f32.const 0x0123456789ABCDEFp-019) drop))`);

// ./test/core/const.wast:59
let $20 = instantiate(`(module (func (f32.const 0x0123456789ABCDEF.) drop))`);

// ./test/core/const.wast:60
let $21 = instantiate(`(module (func (f32.const 0x0123456789ABCDEF.p019) drop))`);

// ./test/core/const.wast:61
let $22 = instantiate(`(module (func (f32.const 0x0123456789ABCDEF.p+019) drop))`);

// ./test/core/const.wast:62
let $23 = instantiate(`(module (func (f32.const 0x0123456789ABCDEF.p-019) drop))`);

// ./test/core/const.wast:63
let $24 = instantiate(`(module (func (f32.const 0x0123456789ABCDEF.019aF) drop))`);

// ./test/core/const.wast:64
let $25 = instantiate(`(module (func (f32.const 0x0123456789ABCDEF.019aFp019) drop))`);

// ./test/core/const.wast:65
let $26 = instantiate(`(module (func (f32.const 0x0123456789ABCDEF.019aFp+019) drop))`);

// ./test/core/const.wast:66
let $27 = instantiate(`(module (func (f32.const 0x0123456789ABCDEF.019aFp-019) drop))`);

// ./test/core/const.wast:67
assert_malformed(() => instantiate(`(func (f32.const) drop) `), `unexpected token`);

// ./test/core/const.wast:71
assert_malformed(() => instantiate(`(func (f32.const .0) drop) `), `unknown operator`);

// ./test/core/const.wast:75
assert_malformed(() => instantiate(`(func (f32.const .0e0) drop) `), `unknown operator`);

// ./test/core/const.wast:79
assert_malformed(() => instantiate(`(func (f32.const 0e) drop) `), `unknown operator`);

// ./test/core/const.wast:83
assert_malformed(() => instantiate(`(func (f32.const 0e+) drop) `), `unknown operator`);

// ./test/core/const.wast:87
assert_malformed(() => instantiate(`(func (f32.const 0.0e) drop) `), `unknown operator`);

// ./test/core/const.wast:91
assert_malformed(() => instantiate(`(func (f32.const 0.0e-) drop) `), `unknown operator`);

// ./test/core/const.wast:95
assert_malformed(() => instantiate(`(func (f32.const 0x) drop) `), `unknown operator`);

// ./test/core/const.wast:99
assert_malformed(() => instantiate(`(func (f32.const 1x) drop) `), `unknown operator`);

// ./test/core/const.wast:103
assert_malformed(() => instantiate(`(func (f32.const 0xg) drop) `), `unknown operator`);

// ./test/core/const.wast:107
assert_malformed(() => instantiate(`(func (f32.const 0x.) drop) `), `unknown operator`);

// ./test/core/const.wast:111
assert_malformed(() => instantiate(`(func (f32.const 0x0.g) drop) `), `unknown operator`);

// ./test/core/const.wast:115
assert_malformed(() => instantiate(`(func (f32.const 0x0p) drop) `), `unknown operator`);

// ./test/core/const.wast:119
assert_malformed(() => instantiate(`(func (f32.const 0x0p+) drop) `), `unknown operator`);

// ./test/core/const.wast:123
assert_malformed(() => instantiate(`(func (f32.const 0x0p-) drop) `), `unknown operator`);

// ./test/core/const.wast:127
assert_malformed(() => instantiate(`(func (f32.const 0x0.0p) drop) `), `unknown operator`);

// ./test/core/const.wast:131
assert_malformed(() => instantiate(`(func (f32.const 0x0.0p+) drop) `), `unknown operator`);

// ./test/core/const.wast:135
assert_malformed(() => instantiate(`(func (f32.const 0x0.0p-) drop) `), `unknown operator`);

// ./test/core/const.wast:139
assert_malformed(() => instantiate(`(func (f32.const 0x0pA) drop) `), `unknown operator`);

// ./test/core/const.wast:145
let $28 = instantiate(`(module (func (f64.const 0123456789) drop))`);

// ./test/core/const.wast:146
let $29 = instantiate(`(module (func (f64.const 0123456789e019) drop))`);

// ./test/core/const.wast:147
let $30 = instantiate(`(module (func (f64.const 0123456789e+019) drop))`);

// ./test/core/const.wast:148
let $31 = instantiate(`(module (func (f64.const 0123456789e-019) drop))`);

// ./test/core/const.wast:149
let $32 = instantiate(`(module (func (f64.const 0123456789.) drop))`);

// ./test/core/const.wast:150
let $33 = instantiate(`(module (func (f64.const 0123456789.e019) drop))`);

// ./test/core/const.wast:151
let $34 = instantiate(`(module (func (f64.const 0123456789.e+019) drop))`);

// ./test/core/const.wast:152
let $35 = instantiate(`(module (func (f64.const 0123456789.e-019) drop))`);

// ./test/core/const.wast:153
let $36 = instantiate(`(module (func (f64.const 0123456789.0123456789) drop))`);

// ./test/core/const.wast:154
let $37 = instantiate(`(module (func (f64.const 0123456789.0123456789e019) drop))`);

// ./test/core/const.wast:155
let $38 = instantiate(`(module (func (f64.const 0123456789.0123456789e+019) drop))`);

// ./test/core/const.wast:156
let $39 = instantiate(`(module (func (f64.const 0123456789.0123456789e-019) drop))`);

// ./test/core/const.wast:157
let $40 = instantiate(`(module (func (f64.const 0_1_2_3_4_5_6_7_8_9) drop))`);

// ./test/core/const.wast:158
let $41 = instantiate(`(module (func (f64.const 0_1_2_3_4_5_6_7_8_9.) drop))`);

// ./test/core/const.wast:159
let $42 = instantiate(`(module (func (f64.const 0_1_2_3_4_5_6_7_8_9.0_1_2_3_4_5_6_7_8_9) drop))`);

// ./test/core/const.wast:160
let $43 = instantiate(`(module (func (f64.const 0_1_2_3_4_5_6_7_8_9e+0_1_9) drop))`);

// ./test/core/const.wast:161
let $44 = instantiate(`(module (func (f64.const 0_1_2_3_4_5_6_7_8_9.e+0_1_9) drop))`);

// ./test/core/const.wast:162
let $45 = instantiate(`(module (func (f64.const 0_1_2_3_4_5_6_7_8_9.0_1_2_3_4_5_6_7_8_9e0_1_9) drop))`);

// ./test/core/const.wast:164
let $46 = instantiate(`(module (func (f64.const 0x0123456789ABCDEFabcdef) drop))`);

// ./test/core/const.wast:165
let $47 = instantiate(`(module (func (f64.const 0x0123456789ABCDEFabcdefp019) drop))`);

// ./test/core/const.wast:166
let $48 = instantiate(`(module (func (f64.const 0x0123456789ABCDEFabcdefp+019) drop))`);

// ./test/core/const.wast:167
let $49 = instantiate(`(module (func (f64.const 0x0123456789ABCDEFabcdefp-019) drop))`);

// ./test/core/const.wast:168
let $50 = instantiate(`(module (func (f64.const 0x0123456789ABCDEFabcdef.) drop))`);

// ./test/core/const.wast:169
let $51 = instantiate(`(module (func (f64.const 0x0123456789ABCDEFabcdef.p019) drop))`);

// ./test/core/const.wast:170
let $52 = instantiate(`(module (func (f64.const 0x0123456789ABCDEFabcdef.p+019) drop))`);

// ./test/core/const.wast:171
let $53 = instantiate(`(module (func (f64.const 0x0123456789ABCDEFabcdef.p-019) drop))`);

// ./test/core/const.wast:172
let $54 = instantiate(`(module (func (f64.const 0x0123456789ABCDEFabcdef.0123456789ABCDEFabcdef) drop))`);

// ./test/core/const.wast:173
let $55 = instantiate(`(module (func (f64.const 0x0123456789ABCDEFabcdef.0123456789ABCDEFabcdefp019) drop))`);

// ./test/core/const.wast:174
let $56 = instantiate(`(module (func (f64.const 0x0123456789ABCDEFabcdef.0123456789ABCDEFabcdefp+019) drop))`);

// ./test/core/const.wast:175
let $57 = instantiate(`(module (func (f64.const 0x0123456789ABCDEFabcdef.0123456789ABCDEFabcdefp-019) drop))`);

// ./test/core/const.wast:176
let $58 = instantiate(`(module (func (f64.const 0x0_1_2_3_4_5_6_7_8_9_A_B_C_D_E_F_a_b_c_d_e_f) drop))`);

// ./test/core/const.wast:177
let $59 = instantiate(`(module (func (f64.const 0x0_1_2_3_4_5_6_7_8_9_A_B_C_D_E_F_a_b_c_d_e_f.) drop))`);

// ./test/core/const.wast:178
let $60 = instantiate(`(module (func (f64.const 0x0_1_2_3_4_5_6_7_8_9_A_B_C_D_E_F_a_b_c_d_e_f.0_1_2_3_4_5_6_7_8_9_A_B_C_D_E_F_a_b_c_d_e_f) drop))`);

// ./test/core/const.wast:179
let $61 = instantiate(`(module (func (f64.const 0x0_1_2_3_4_5_6_7_8_9_A_B_C_D_E_F_a_b_c_d_e_fp0_1_9) drop))`);

// ./test/core/const.wast:180
let $62 = instantiate(`(module (func (f64.const 0x0_1_2_3_4_5_6_7_8_9_A_B_C_D_E_F_a_b_c_d_e_f.p0_1_9) drop))`);

// ./test/core/const.wast:181
let $63 = instantiate(`(module (func (f64.const 0x0_1_2_3_4_5_6_7_8_9_A_B_C_D_E_F_a_b_c_d_e_f.0_1_2_3_4_5_6_7_8_9_A_B_C_D_E_F_a_b_c_d_e_fp0_1_9) drop))`);

// ./test/core/const.wast:184
assert_malformed(() => instantiate(`(func (f64.const) drop) `), `unexpected token`);

// ./test/core/const.wast:188
assert_malformed(() => instantiate(`(func (f64.const .0) drop) `), `unknown operator`);

// ./test/core/const.wast:192
assert_malformed(() => instantiate(`(func (f64.const .0e0) drop) `), `unknown operator`);

// ./test/core/const.wast:196
assert_malformed(() => instantiate(`(func (f64.const 0e) drop) `), `unknown operator`);

// ./test/core/const.wast:200
assert_malformed(() => instantiate(`(func (f64.const 0e+) drop) `), `unknown operator`);

// ./test/core/const.wast:204
assert_malformed(() => instantiate(`(func (f64.const 0.0e) drop) `), `unknown operator`);

// ./test/core/const.wast:208
assert_malformed(() => instantiate(`(func (f64.const 0.0e-) drop) `), `unknown operator`);

// ./test/core/const.wast:212
assert_malformed(() => instantiate(`(func (f64.const 0x) drop) `), `unknown operator`);

// ./test/core/const.wast:216
assert_malformed(() => instantiate(`(func (f64.const 1x) drop) `), `unknown operator`);

// ./test/core/const.wast:220
assert_malformed(() => instantiate(`(func (f64.const 0xg) drop) `), `unknown operator`);

// ./test/core/const.wast:224
assert_malformed(() => instantiate(`(func (f64.const 0x.) drop) `), `unknown operator`);

// ./test/core/const.wast:228
assert_malformed(() => instantiate(`(func (f64.const 0x0.g) drop) `), `unknown operator`);

// ./test/core/const.wast:232
assert_malformed(() => instantiate(`(func (f64.const 0x0p) drop) `), `unknown operator`);

// ./test/core/const.wast:236
assert_malformed(() => instantiate(`(func (f64.const 0x0p+) drop) `), `unknown operator`);

// ./test/core/const.wast:240
assert_malformed(() => instantiate(`(func (f64.const 0x0p-) drop) `), `unknown operator`);

// ./test/core/const.wast:244
assert_malformed(() => instantiate(`(func (f64.const 0x0.0p) drop) `), `unknown operator`);

// ./test/core/const.wast:248
assert_malformed(() => instantiate(`(func (f64.const 0x0.0p+) drop) `), `unknown operator`);

// ./test/core/const.wast:252
assert_malformed(() => instantiate(`(func (f64.const 0x0.0p-) drop) `), `unknown operator`);

// ./test/core/const.wast:256
assert_malformed(() => instantiate(`(func (f64.const 0x0pA) drop) `), `unknown operator`);

// ./test/core/const.wast:264
let $64 = instantiate(`(module (func (i32.const 0xffffffff) drop))`);

// ./test/core/const.wast:265
let $65 = instantiate(`(module (func (i32.const -0x80000000) drop))`);

// ./test/core/const.wast:266
assert_malformed(() => instantiate(`(func (i32.const 0x100000000) drop) `), `constant out of range`);

// ./test/core/const.wast:270
assert_malformed(() => instantiate(`(func (i32.const -0x80000001) drop) `), `constant out of range`);

// ./test/core/const.wast:275
let $66 = instantiate(`(module (func (i32.const 4294967295) drop))`);

// ./test/core/const.wast:276
let $67 = instantiate(`(module (func (i32.const -2147483648) drop))`);

// ./test/core/const.wast:277
assert_malformed(() => instantiate(`(func (i32.const 4294967296) drop) `), `constant out of range`);

// ./test/core/const.wast:281
assert_malformed(() => instantiate(`(func (i32.const -2147483649) drop) `), `constant out of range`);

// ./test/core/const.wast:286
let $68 = instantiate(`(module (func (i64.const 0xffffffffffffffff) drop))`);

// ./test/core/const.wast:287
let $69 = instantiate(`(module (func (i64.const -0x8000000000000000) drop))`);

// ./test/core/const.wast:288
assert_malformed(() => instantiate(`(func (i64.const 0x10000000000000000) drop) `), `constant out of range`);

// ./test/core/const.wast:292
assert_malformed(() => instantiate(`(func (i64.const -0x8000000000000001) drop) `), `constant out of range`);

// ./test/core/const.wast:297
let $70 = instantiate(`(module (func (i64.const 18446744073709551615) drop))`);

// ./test/core/const.wast:298
let $71 = instantiate(`(module (func (i64.const -9223372036854775808) drop))`);

// ./test/core/const.wast:299
assert_malformed(() => instantiate(`(func (i64.const 18446744073709551616) drop) `), `constant out of range`);

// ./test/core/const.wast:303
assert_malformed(() => instantiate(`(func (i64.const -9223372036854775809) drop) `), `constant out of range`);

// ./test/core/const.wast:308
let $72 = instantiate(`(module (func (f32.const 0x1p127) drop))`);

// ./test/core/const.wast:309
let $73 = instantiate(`(module (func (f32.const -0x1p127) drop))`);

// ./test/core/const.wast:310
let $74 = instantiate(`(module (func (f32.const 0x1.fffffep127) drop))`);

// ./test/core/const.wast:311
let $75 = instantiate(`(module (func (f32.const -0x1.fffffep127) drop))`);

// ./test/core/const.wast:312
let $76 = instantiate(`(module (func (f32.const 0x1.fffffe7p127) drop))`);

// ./test/core/const.wast:313
let $77 = instantiate(`(module (func (f32.const -0x1.fffffe7p127) drop))`);

// ./test/core/const.wast:314
let $78 = instantiate(`(module (func (f32.const 0x1.fffffefffffff8000000p127) drop))`);

// ./test/core/const.wast:315
let $79 = instantiate(`(module (func (f32.const -0x1.fffffefffffff8000000p127) drop))`);

// ./test/core/const.wast:316
let $80 = instantiate(`(module (func (f32.const 0x1.fffffefffffffffffffp127) drop))`);

// ./test/core/const.wast:317
let $81 = instantiate(`(module (func (f32.const -0x1.fffffefffffffffffffp127) drop))`);

// ./test/core/const.wast:318
assert_malformed(() => instantiate(`(func (f32.const 0x1p128) drop) `), `constant out of range`);

// ./test/core/const.wast:322
assert_malformed(() => instantiate(`(func (f32.const -0x1p128) drop) `), `constant out of range`);

// ./test/core/const.wast:326
assert_malformed(() => instantiate(`(func (f32.const 0x1.ffffffp127) drop) `), `constant out of range`);

// ./test/core/const.wast:330
assert_malformed(() => instantiate(`(func (f32.const -0x1.ffffffp127) drop) `), `constant out of range`);

// ./test/core/const.wast:335
let $82 = instantiate(`(module (func (f32.const 1e38) drop))`);

// ./test/core/const.wast:336
let $83 = instantiate(`(module (func (f32.const -1e38) drop))`);

// ./test/core/const.wast:337
assert_malformed(() => instantiate(`(func (f32.const 1e39) drop) `), `constant out of range`);

// ./test/core/const.wast:341
assert_malformed(() => instantiate(`(func (f32.const -1e39) drop) `), `constant out of range`);

// ./test/core/const.wast:346
let $84 = instantiate(`(module (func (f32.const 340282356779733623858607532500980858880) drop))`);

// ./test/core/const.wast:347
let $85 = instantiate(`(module (func (f32.const -340282356779733623858607532500980858880) drop))`);

// ./test/core/const.wast:348
assert_malformed(() => instantiate(`(func (f32.const 340282356779733661637539395458142568448) drop) `), `constant out of range`);

// ./test/core/const.wast:352
assert_malformed(() => instantiate(`(func (f32.const -340282356779733661637539395458142568448) drop) `), `constant out of range`);

// ./test/core/const.wast:357
let $86 = instantiate(`(module (func (f64.const 0x1p1023) drop))`);

// ./test/core/const.wast:358
let $87 = instantiate(`(module (func (f64.const -0x1p1023) drop))`);

// ./test/core/const.wast:359
let $88 = instantiate(`(module (func (f64.const 0x1.fffffffffffffp1023) drop))`);

// ./test/core/const.wast:360
let $89 = instantiate(`(module (func (f64.const -0x1.fffffffffffffp1023) drop))`);

// ./test/core/const.wast:361
let $90 = instantiate(`(module (func (f64.const 0x1.fffffffffffff7p1023) drop))`);

// ./test/core/const.wast:362
let $91 = instantiate(`(module (func (f64.const -0x1.fffffffffffff7p1023) drop))`);

// ./test/core/const.wast:363
let $92 = instantiate(`(module (func (f64.const 0x1.fffffffffffff7ffffffp1023) drop))`);

// ./test/core/const.wast:364
let $93 = instantiate(`(module (func (f64.const -0x1.fffffffffffff7ffffffp1023) drop))`);

// ./test/core/const.wast:365
assert_malformed(() => instantiate(`(func (f64.const 0x1p1024) drop) `), `constant out of range`);

// ./test/core/const.wast:369
assert_malformed(() => instantiate(`(func (f64.const -0x1p1024) drop) `), `constant out of range`);

// ./test/core/const.wast:373
assert_malformed(() => instantiate(`(func (f64.const 0x1.fffffffffffff8p1023) drop) `), `constant out of range`);

// ./test/core/const.wast:377
assert_malformed(() => instantiate(`(func (f64.const -0x1.fffffffffffff8p1023) drop) `), `constant out of range`);

// ./test/core/const.wast:382
let $94 = instantiate(`(module (func (f64.const 1e308) drop))`);

// ./test/core/const.wast:383
let $95 = instantiate(`(module (func (f64.const -1e308) drop))`);

// ./test/core/const.wast:384
assert_malformed(() => instantiate(`(func (f64.const 1e309) drop) `), `constant out of range`);

// ./test/core/const.wast:388
assert_malformed(() => instantiate(`(func (f64.const -1e309) drop) `), `constant out of range`);

// ./test/core/const.wast:393
let $96 = instantiate(`(module (func (f64.const 179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858368) drop))`);

// ./test/core/const.wast:394
let $97 = instantiate(`(module (func (f64.const -179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858368) drop))`);

// ./test/core/const.wast:395
assert_malformed(() => instantiate(`(func (f64.const 269653970229347356221791135597556535197105851288767494898376215204735891170042808140884337949150317257310688430271573696351481990334196274152701320055306275479074865864826923114368235135583993416113802762682700913456874855354834422248712838998185022412196739306217084753107265771378949821875606039276187287552) drop) `), `constant out of range`);

// ./test/core/const.wast:399
assert_malformed(() => instantiate(`(func (f64.const -269653970229347356221791135597556535197105851288767494898376215204735891170042808140884337949150317257310688430271573696351481990334196274152701320055306275479074865864826923114368235135583993416113802762682700913456874855354834422248712838998185022412196739306217084753107265771378949821875606039276187287552) drop) `), `constant out of range`);

// ./test/core/const.wast:404
let $98 = instantiate(`(module (func (f32.const nan:0x1) drop))`);

// ./test/core/const.wast:405
let $99 = instantiate(`(module (func (f64.const nan:0x1) drop))`);

// ./test/core/const.wast:406
let $100 = instantiate(`(module (func (f32.const nan:0x7f_ffff) drop))`);

// ./test/core/const.wast:407
let $101 = instantiate(`(module (func (f64.const nan:0xf_ffff_ffff_ffff) drop))`);

// ./test/core/const.wast:409
assert_malformed(() => instantiate(`(func (f32.const nan:1) drop) `), `unknown operator`);

// ./test/core/const.wast:413
assert_malformed(() => instantiate(`(func (f64.const nan:1) drop) `), `unknown operator`);

// ./test/core/const.wast:418
assert_malformed(() => instantiate(`(func (f32.const nan:0x0) drop) `), `constant out of range`);

// ./test/core/const.wast:422
assert_malformed(() => instantiate(`(func (f64.const nan:0x0) drop) `), `constant out of range`);

// ./test/core/const.wast:427
assert_malformed(() => instantiate(`(func (f32.const nan:0x80_0000) drop) `), `constant out of range`);

// ./test/core/const.wast:431
assert_malformed(() => instantiate(`(func (f64.const nan:0x10_0000_0000_0000) drop) `), `constant out of range`);

// ./test/core/const.wast:440
let $102 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.00000100000000000p-50)))`);

// ./test/core/const.wast:441
assert_return(() => invoke($102, `f`, []), [value('f32', 0.0000000000000008881784)]);

// ./test/core/const.wast:442
let $103 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.00000100000000000p-50)))`);

// ./test/core/const.wast:443
assert_return(() => invoke($103, `f`, []), [value('f32', -0.0000000000000008881784)]);

// ./test/core/const.wast:444
let $104 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.00000100000000001p-50)))`);

// ./test/core/const.wast:445
assert_return(() => invoke($104, `f`, []), [value('f32', 0.0000000000000008881785)]);

// ./test/core/const.wast:446
let $105 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.00000100000000001p-50)))`);

// ./test/core/const.wast:447
assert_return(() => invoke($105, `f`, []), [value('f32', -0.0000000000000008881785)]);

// ./test/core/const.wast:448
let $106 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.000001fffffffffffp-50)))`);

// ./test/core/const.wast:449
assert_return(() => invoke($106, `f`, []), [value('f32', 0.0000000000000008881785)]);

// ./test/core/const.wast:450
let $107 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.000001fffffffffffp-50)))`);

// ./test/core/const.wast:451
assert_return(() => invoke($107, `f`, []), [value('f32', -0.0000000000000008881785)]);

// ./test/core/const.wast:452
let $108 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.00000200000000000p-50)))`);

// ./test/core/const.wast:453
assert_return(() => invoke($108, `f`, []), [value('f32', 0.0000000000000008881785)]);

// ./test/core/const.wast:454
let $109 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.00000200000000000p-50)))`);

// ./test/core/const.wast:455
assert_return(() => invoke($109, `f`, []), [value('f32', -0.0000000000000008881785)]);

// ./test/core/const.wast:456
let $110 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.00000200000000001p-50)))`);

// ./test/core/const.wast:457
assert_return(() => invoke($110, `f`, []), [value('f32', 0.0000000000000008881785)]);

// ./test/core/const.wast:458
let $111 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.00000200000000001p-50)))`);

// ./test/core/const.wast:459
assert_return(() => invoke($111, `f`, []), [value('f32', -0.0000000000000008881785)]);

// ./test/core/const.wast:460
let $112 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.000002fffffffffffp-50)))`);

// ./test/core/const.wast:461
assert_return(() => invoke($112, `f`, []), [value('f32', 0.0000000000000008881785)]);

// ./test/core/const.wast:462
let $113 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.000002fffffffffffp-50)))`);

// ./test/core/const.wast:463
assert_return(() => invoke($113, `f`, []), [value('f32', -0.0000000000000008881785)]);

// ./test/core/const.wast:464
let $114 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.00000300000000000p-50)))`);

// ./test/core/const.wast:465
assert_return(() => invoke($114, `f`, []), [value('f32', 0.0000000000000008881786)]);

// ./test/core/const.wast:466
let $115 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.00000300000000000p-50)))`);

// ./test/core/const.wast:467
assert_return(() => invoke($115, `f`, []), [value('f32', -0.0000000000000008881786)]);

// ./test/core/const.wast:468
let $116 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.00000300000000001p-50)))`);

// ./test/core/const.wast:469
assert_return(() => invoke($116, `f`, []), [value('f32', 0.0000000000000008881786)]);

// ./test/core/const.wast:470
let $117 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.00000300000000001p-50)))`);

// ./test/core/const.wast:471
assert_return(() => invoke($117, `f`, []), [value('f32', -0.0000000000000008881786)]);

// ./test/core/const.wast:472
let $118 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.000003fffffffffffp-50)))`);

// ./test/core/const.wast:473
assert_return(() => invoke($118, `f`, []), [value('f32', 0.0000000000000008881786)]);

// ./test/core/const.wast:474
let $119 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.000003fffffffffffp-50)))`);

// ./test/core/const.wast:475
assert_return(() => invoke($119, `f`, []), [value('f32', -0.0000000000000008881786)]);

// ./test/core/const.wast:476
let $120 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.00000400000000000p-50)))`);

// ./test/core/const.wast:477
assert_return(() => invoke($120, `f`, []), [value('f32', 0.0000000000000008881786)]);

// ./test/core/const.wast:478
let $121 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.00000400000000000p-50)))`);

// ./test/core/const.wast:479
assert_return(() => invoke($121, `f`, []), [value('f32', -0.0000000000000008881786)]);

// ./test/core/const.wast:480
let $122 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.00000400000000001p-50)))`);

// ./test/core/const.wast:481
assert_return(() => invoke($122, `f`, []), [value('f32', 0.0000000000000008881786)]);

// ./test/core/const.wast:482
let $123 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.00000400000000001p-50)))`);

// ./test/core/const.wast:483
assert_return(() => invoke($123, `f`, []), [value('f32', -0.0000000000000008881786)]);

// ./test/core/const.wast:484
let $124 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.000004fffffffffffp-50)))`);

// ./test/core/const.wast:485
assert_return(() => invoke($124, `f`, []), [value('f32', 0.0000000000000008881786)]);

// ./test/core/const.wast:486
let $125 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.000004fffffffffffp-50)))`);

// ./test/core/const.wast:487
assert_return(() => invoke($125, `f`, []), [value('f32', -0.0000000000000008881786)]);

// ./test/core/const.wast:488
let $126 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.00000500000000000p-50)))`);

// ./test/core/const.wast:489
assert_return(() => invoke($126, `f`, []), [value('f32', 0.0000000000000008881786)]);

// ./test/core/const.wast:490
let $127 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.00000500000000000p-50)))`);

// ./test/core/const.wast:491
assert_return(() => invoke($127, `f`, []), [value('f32', -0.0000000000000008881786)]);

// ./test/core/const.wast:492
let $128 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.00000500000000001p-50)))`);

// ./test/core/const.wast:493
assert_return(() => invoke($128, `f`, []), [value('f32', 0.0000000000000008881787)]);

// ./test/core/const.wast:494
let $129 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.00000500000000001p-50)))`);

// ./test/core/const.wast:495
assert_return(() => invoke($129, `f`, []), [value('f32', -0.0000000000000008881787)]);

// ./test/core/const.wast:497
let $130 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x4000.004000000p-64)))`);

// ./test/core/const.wast:498
assert_return(() => invoke($130, `f`, []), [value('f32', 0.0000000000000008881784)]);

// ./test/core/const.wast:499
let $131 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x4000.004000000p-64)))`);

// ./test/core/const.wast:500
assert_return(() => invoke($131, `f`, []), [value('f32', -0.0000000000000008881784)]);

// ./test/core/const.wast:501
let $132 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x4000.004000001p-64)))`);

// ./test/core/const.wast:502
assert_return(() => invoke($132, `f`, []), [value('f32', 0.0000000000000008881785)]);

// ./test/core/const.wast:503
let $133 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x4000.004000001p-64)))`);

// ./test/core/const.wast:504
assert_return(() => invoke($133, `f`, []), [value('f32', -0.0000000000000008881785)]);

// ./test/core/const.wast:505
let $134 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x4000.007ffffffp-64)))`);

// ./test/core/const.wast:506
assert_return(() => invoke($134, `f`, []), [value('f32', 0.0000000000000008881785)]);

// ./test/core/const.wast:507
let $135 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x4000.007ffffffp-64)))`);

// ./test/core/const.wast:508
assert_return(() => invoke($135, `f`, []), [value('f32', -0.0000000000000008881785)]);

// ./test/core/const.wast:509
let $136 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x4000.008000000p-64)))`);

// ./test/core/const.wast:510
assert_return(() => invoke($136, `f`, []), [value('f32', 0.0000000000000008881785)]);

// ./test/core/const.wast:511
let $137 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x4000.008000000p-64)))`);

// ./test/core/const.wast:512
assert_return(() => invoke($137, `f`, []), [value('f32', -0.0000000000000008881785)]);

// ./test/core/const.wast:513
let $138 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x4000.008000001p-64)))`);

// ./test/core/const.wast:514
assert_return(() => invoke($138, `f`, []), [value('f32', 0.0000000000000008881785)]);

// ./test/core/const.wast:515
let $139 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x4000.008000001p-64)))`);

// ./test/core/const.wast:516
assert_return(() => invoke($139, `f`, []), [value('f32', -0.0000000000000008881785)]);

// ./test/core/const.wast:517
let $140 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x4000.00bffffffp-64)))`);

// ./test/core/const.wast:518
assert_return(() => invoke($140, `f`, []), [value('f32', 0.0000000000000008881785)]);

// ./test/core/const.wast:519
let $141 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x4000.00bffffffp-64)))`);

// ./test/core/const.wast:520
assert_return(() => invoke($141, `f`, []), [value('f32', -0.0000000000000008881785)]);

// ./test/core/const.wast:521
let $142 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x4000.00c000000p-64)))`);

// ./test/core/const.wast:522
assert_return(() => invoke($142, `f`, []), [value('f32', 0.0000000000000008881786)]);

// ./test/core/const.wast:523
let $143 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x4000.00c000000p-64)))`);

// ./test/core/const.wast:524
assert_return(() => invoke($143, `f`, []), [value('f32', -0.0000000000000008881786)]);

// ./test/core/const.wast:525
let $144 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x4000.00c000001p-64)))`);

// ./test/core/const.wast:526
assert_return(() => invoke($144, `f`, []), [value('f32', 0.0000000000000008881786)]);

// ./test/core/const.wast:527
let $145 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x4000.00c000001p-64)))`);

// ./test/core/const.wast:528
assert_return(() => invoke($145, `f`, []), [value('f32', -0.0000000000000008881786)]);

// ./test/core/const.wast:529
let $146 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x4000.00fffffffp-64)))`);

// ./test/core/const.wast:530
assert_return(() => invoke($146, `f`, []), [value('f32', 0.0000000000000008881786)]);

// ./test/core/const.wast:531
let $147 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x4000.00fffffffp-64)))`);

// ./test/core/const.wast:532
assert_return(() => invoke($147, `f`, []), [value('f32', -0.0000000000000008881786)]);

// ./test/core/const.wast:533
let $148 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x4000.010000001p-64)))`);

// ./test/core/const.wast:534
assert_return(() => invoke($148, `f`, []), [value('f32', 0.0000000000000008881786)]);

// ./test/core/const.wast:535
let $149 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x4000.010000001p-64)))`);

// ./test/core/const.wast:536
assert_return(() => invoke($149, `f`, []), [value('f32', -0.0000000000000008881786)]);

// ./test/core/const.wast:537
let $150 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x4000.013ffffffp-64)))`);

// ./test/core/const.wast:538
assert_return(() => invoke($150, `f`, []), [value('f32', 0.0000000000000008881786)]);

// ./test/core/const.wast:539
let $151 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x4000.013ffffffp-64)))`);

// ./test/core/const.wast:540
assert_return(() => invoke($151, `f`, []), [value('f32', -0.0000000000000008881786)]);

// ./test/core/const.wast:541
let $152 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x4000.014000001p-64)))`);

// ./test/core/const.wast:542
assert_return(() => invoke($152, `f`, []), [value('f32', 0.0000000000000008881787)]);

// ./test/core/const.wast:543
let $153 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x4000.014000001p-64)))`);

// ./test/core/const.wast:544
assert_return(() => invoke($153, `f`, []), [value('f32', -0.0000000000000008881787)]);

// ./test/core/const.wast:546
let $154 = instantiate(`(module (func (export "f") (result f32) (f32.const +8.8817847263968443573e-16)))`);

// ./test/core/const.wast:547
assert_return(() => invoke($154, `f`, []), [value('f32', 0.0000000000000008881784)]);

// ./test/core/const.wast:548
let $155 = instantiate(`(module (func (export "f") (result f32) (f32.const -8.8817847263968443573e-16)))`);

// ./test/core/const.wast:549
assert_return(() => invoke($155, `f`, []), [value('f32', -0.0000000000000008881784)]);

// ./test/core/const.wast:550
let $156 = instantiate(`(module (func (export "f") (result f32) (f32.const +8.8817847263968443574e-16)))`);

// ./test/core/const.wast:551
assert_return(() => invoke($156, `f`, []), [value('f32', 0.0000000000000008881785)]);

// ./test/core/const.wast:552
let $157 = instantiate(`(module (func (export "f") (result f32) (f32.const -8.8817847263968443574e-16)))`);

// ./test/core/const.wast:553
assert_return(() => invoke($157, `f`, []), [value('f32', -0.0000000000000008881785)]);

// ./test/core/const.wast:554
let $158 = instantiate(`(module (func (export "f") (result f32) (f32.const +8.8817857851880284252e-16)))`);

// ./test/core/const.wast:555
assert_return(() => invoke($158, `f`, []), [value('f32', 0.0000000000000008881785)]);

// ./test/core/const.wast:556
let $159 = instantiate(`(module (func (export "f") (result f32) (f32.const -8.8817857851880284252e-16)))`);

// ./test/core/const.wast:557
assert_return(() => invoke($159, `f`, []), [value('f32', -0.0000000000000008881785)]);

// ./test/core/const.wast:558
let $160 = instantiate(`(module (func (export "f") (result f32) (f32.const +8.8817857851880284253e-16)))`);

// ./test/core/const.wast:559
assert_return(() => invoke($160, `f`, []), [value('f32', 0.0000000000000008881786)]);

// ./test/core/const.wast:560
let $161 = instantiate(`(module (func (export "f") (result f32) (f32.const -8.8817857851880284253e-16)))`);

// ./test/core/const.wast:561
assert_return(() => invoke($161, `f`, []), [value('f32', -0.0000000000000008881786)]);

// ./test/core/const.wast:564
let $162 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.00000100000000000p+50)))`);

// ./test/core/const.wast:565
assert_return(() => invoke($162, `f`, []), [value('f32', 1125899900000000)]);

// ./test/core/const.wast:566
let $163 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.00000100000000000p+50)))`);

// ./test/core/const.wast:567
assert_return(() => invoke($163, `f`, []), [value('f32', -1125899900000000)]);

// ./test/core/const.wast:568
let $164 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.00000100000000001p+50)))`);

// ./test/core/const.wast:569
assert_return(() => invoke($164, `f`, []), [value('f32', 1125900000000000)]);

// ./test/core/const.wast:570
let $165 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.00000100000000001p+50)))`);

// ./test/core/const.wast:571
assert_return(() => invoke($165, `f`, []), [value('f32', -1125900000000000)]);

// ./test/core/const.wast:572
let $166 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.000001fffffffffffp+50)))`);

// ./test/core/const.wast:573
assert_return(() => invoke($166, `f`, []), [value('f32', 1125900000000000)]);

// ./test/core/const.wast:574
let $167 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.000001fffffffffffp+50)))`);

// ./test/core/const.wast:575
assert_return(() => invoke($167, `f`, []), [value('f32', -1125900000000000)]);

// ./test/core/const.wast:576
let $168 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.00000200000000000p+50)))`);

// ./test/core/const.wast:577
assert_return(() => invoke($168, `f`, []), [value('f32', 1125900000000000)]);

// ./test/core/const.wast:578
let $169 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.00000200000000000p+50)))`);

// ./test/core/const.wast:579
assert_return(() => invoke($169, `f`, []), [value('f32', -1125900000000000)]);

// ./test/core/const.wast:580
let $170 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.00000200000000001p+50)))`);

// ./test/core/const.wast:581
assert_return(() => invoke($170, `f`, []), [value('f32', 1125900000000000)]);

// ./test/core/const.wast:582
let $171 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.00000200000000001p+50)))`);

// ./test/core/const.wast:583
assert_return(() => invoke($171, `f`, []), [value('f32', -1125900000000000)]);

// ./test/core/const.wast:584
let $172 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.000002fffffffffffp+50)))`);

// ./test/core/const.wast:585
assert_return(() => invoke($172, `f`, []), [value('f32', 1125900000000000)]);

// ./test/core/const.wast:586
let $173 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.000002fffffffffffp+50)))`);

// ./test/core/const.wast:587
assert_return(() => invoke($173, `f`, []), [value('f32', -1125900000000000)]);

// ./test/core/const.wast:588
let $174 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.00000300000000000p+50)))`);

// ./test/core/const.wast:589
assert_return(() => invoke($174, `f`, []), [value('f32', 1125900200000000)]);

// ./test/core/const.wast:590
let $175 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.00000300000000000p+50)))`);

// ./test/core/const.wast:591
assert_return(() => invoke($175, `f`, []), [value('f32', -1125900200000000)]);

// ./test/core/const.wast:592
let $176 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.00000300000000001p+50)))`);

// ./test/core/const.wast:593
assert_return(() => invoke($176, `f`, []), [value('f32', 1125900200000000)]);

// ./test/core/const.wast:594
let $177 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.00000300000000001p+50)))`);

// ./test/core/const.wast:595
assert_return(() => invoke($177, `f`, []), [value('f32', -1125900200000000)]);

// ./test/core/const.wast:596
let $178 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.000003fffffffffffp+50)))`);

// ./test/core/const.wast:597
assert_return(() => invoke($178, `f`, []), [value('f32', 1125900200000000)]);

// ./test/core/const.wast:598
let $179 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.000003fffffffffffp+50)))`);

// ./test/core/const.wast:599
assert_return(() => invoke($179, `f`, []), [value('f32', -1125900200000000)]);

// ./test/core/const.wast:600
let $180 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.00000400000000000p+50)))`);

// ./test/core/const.wast:601
assert_return(() => invoke($180, `f`, []), [value('f32', 1125900200000000)]);

// ./test/core/const.wast:602
let $181 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.00000400000000000p+50)))`);

// ./test/core/const.wast:603
assert_return(() => invoke($181, `f`, []), [value('f32', -1125900200000000)]);

// ./test/core/const.wast:604
let $182 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.00000400000000001p+50)))`);

// ./test/core/const.wast:605
assert_return(() => invoke($182, `f`, []), [value('f32', 1125900200000000)]);

// ./test/core/const.wast:606
let $183 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.00000400000000001p+50)))`);

// ./test/core/const.wast:607
assert_return(() => invoke($183, `f`, []), [value('f32', -1125900200000000)]);

// ./test/core/const.wast:608
let $184 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.000004fffffffffffp+50)))`);

// ./test/core/const.wast:609
assert_return(() => invoke($184, `f`, []), [value('f32', 1125900200000000)]);

// ./test/core/const.wast:610
let $185 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.000004fffffffffffp+50)))`);

// ./test/core/const.wast:611
assert_return(() => invoke($185, `f`, []), [value('f32', -1125900200000000)]);

// ./test/core/const.wast:612
let $186 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.00000500000000000p+50)))`);

// ./test/core/const.wast:613
assert_return(() => invoke($186, `f`, []), [value('f32', 1125900200000000)]);

// ./test/core/const.wast:614
let $187 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.00000500000000000p+50)))`);

// ./test/core/const.wast:615
assert_return(() => invoke($187, `f`, []), [value('f32', -1125900200000000)]);

// ./test/core/const.wast:616
let $188 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.00000500000000001p+50)))`);

// ./test/core/const.wast:617
assert_return(() => invoke($188, `f`, []), [value('f32', 1125900300000000)]);

// ./test/core/const.wast:618
let $189 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.00000500000000001p+50)))`);

// ./test/core/const.wast:619
assert_return(() => invoke($189, `f`, []), [value('f32', -1125900300000000)]);

// ./test/core/const.wast:621
let $190 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x4000004000000)))`);

// ./test/core/const.wast:622
assert_return(() => invoke($190, `f`, []), [value('f32', 1125899900000000)]);

// ./test/core/const.wast:623
let $191 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x4000004000000)))`);

// ./test/core/const.wast:624
assert_return(() => invoke($191, `f`, []), [value('f32', -1125899900000000)]);

// ./test/core/const.wast:625
let $192 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x4000004000001)))`);

// ./test/core/const.wast:626
assert_return(() => invoke($192, `f`, []), [value('f32', 1125900000000000)]);

// ./test/core/const.wast:627
let $193 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x4000004000001)))`);

// ./test/core/const.wast:628
assert_return(() => invoke($193, `f`, []), [value('f32', -1125900000000000)]);

// ./test/core/const.wast:629
let $194 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x4000007ffffff)))`);

// ./test/core/const.wast:630
assert_return(() => invoke($194, `f`, []), [value('f32', 1125900000000000)]);

// ./test/core/const.wast:631
let $195 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x4000007ffffff)))`);

// ./test/core/const.wast:632
assert_return(() => invoke($195, `f`, []), [value('f32', -1125900000000000)]);

// ./test/core/const.wast:633
let $196 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x4000008000000)))`);

// ./test/core/const.wast:634
assert_return(() => invoke($196, `f`, []), [value('f32', 1125900000000000)]);

// ./test/core/const.wast:635
let $197 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x4000008000000)))`);

// ./test/core/const.wast:636
assert_return(() => invoke($197, `f`, []), [value('f32', -1125900000000000)]);

// ./test/core/const.wast:637
let $198 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x4000008000001)))`);

// ./test/core/const.wast:638
assert_return(() => invoke($198, `f`, []), [value('f32', 1125900000000000)]);

// ./test/core/const.wast:639
let $199 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x4000008000001)))`);

// ./test/core/const.wast:640
assert_return(() => invoke($199, `f`, []), [value('f32', -1125900000000000)]);

// ./test/core/const.wast:641
let $200 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x400000bffffff)))`);

// ./test/core/const.wast:642
assert_return(() => invoke($200, `f`, []), [value('f32', 1125900000000000)]);

// ./test/core/const.wast:643
let $201 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x400000bffffff)))`);

// ./test/core/const.wast:644
assert_return(() => invoke($201, `f`, []), [value('f32', -1125900000000000)]);

// ./test/core/const.wast:645
let $202 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x400000c000000)))`);

// ./test/core/const.wast:646
assert_return(() => invoke($202, `f`, []), [value('f32', 1125900200000000)]);

// ./test/core/const.wast:647
let $203 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x400000c000000)))`);

// ./test/core/const.wast:648
assert_return(() => invoke($203, `f`, []), [value('f32', -1125900200000000)]);

// ./test/core/const.wast:650
let $204 = instantiate(`(module (func (export "f") (result f32) (f32.const +1125899973951488)))`);

// ./test/core/const.wast:651
assert_return(() => invoke($204, `f`, []), [value('f32', 1125899900000000)]);

// ./test/core/const.wast:652
let $205 = instantiate(`(module (func (export "f") (result f32) (f32.const -1125899973951488)))`);

// ./test/core/const.wast:653
assert_return(() => invoke($205, `f`, []), [value('f32', -1125899900000000)]);

// ./test/core/const.wast:654
let $206 = instantiate(`(module (func (export "f") (result f32) (f32.const +1125899973951489)))`);

// ./test/core/const.wast:655
assert_return(() => invoke($206, `f`, []), [value('f32', 1125900000000000)]);

// ./test/core/const.wast:656
let $207 = instantiate(`(module (func (export "f") (result f32) (f32.const -1125899973951489)))`);

// ./test/core/const.wast:657
assert_return(() => invoke($207, `f`, []), [value('f32', -1125900000000000)]);

// ./test/core/const.wast:658
let $208 = instantiate(`(module (func (export "f") (result f32) (f32.const +1125900108169215)))`);

// ./test/core/const.wast:659
assert_return(() => invoke($208, `f`, []), [value('f32', 1125900000000000)]);

// ./test/core/const.wast:660
let $209 = instantiate(`(module (func (export "f") (result f32) (f32.const -1125900108169215)))`);

// ./test/core/const.wast:661
assert_return(() => invoke($209, `f`, []), [value('f32', -1125900000000000)]);

// ./test/core/const.wast:662
let $210 = instantiate(`(module (func (export "f") (result f32) (f32.const +1125900108169216)))`);

// ./test/core/const.wast:663
assert_return(() => invoke($210, `f`, []), [value('f32', 1125900200000000)]);

// ./test/core/const.wast:664
let $211 = instantiate(`(module (func (export "f") (result f32) (f32.const -1125900108169216)))`);

// ./test/core/const.wast:665
assert_return(() => invoke($211, `f`, []), [value('f32', -1125900200000000)]);

// ./test/core/const.wast:668
let $212 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x0.00000100000000000p-126)))`);

// ./test/core/const.wast:669
assert_return(() => invoke($212, `f`, []), [value('f32', 0)]);

// ./test/core/const.wast:670
let $213 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x0.00000100000000000p-126)))`);

// ./test/core/const.wast:671
assert_return(() => invoke($213, `f`, []), [bytes('f32', [0x0, 0x0, 0x0, 0x80])]);

// ./test/core/const.wast:672
let $214 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x0.00000100000000001p-126)))`);

// ./test/core/const.wast:673
assert_return(() => invoke($214, `f`, []), [value('f32', 0.000000000000000000000000000000000000000000001)]);

// ./test/core/const.wast:674
let $215 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x0.00000100000000001p-126)))`);

// ./test/core/const.wast:675
assert_return(() => invoke($215, `f`, []), [value('f32', -0.000000000000000000000000000000000000000000001)]);

// ./test/core/const.wast:676
let $216 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x0.000001fffffffffffp-126)))`);

// ./test/core/const.wast:677
assert_return(() => invoke($216, `f`, []), [value('f32', 0.000000000000000000000000000000000000000000001)]);

// ./test/core/const.wast:678
let $217 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x0.000001fffffffffffp-126)))`);

// ./test/core/const.wast:679
assert_return(() => invoke($217, `f`, []), [value('f32', -0.000000000000000000000000000000000000000000001)]);

// ./test/core/const.wast:680
let $218 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x0.00000200000000000p-126)))`);

// ./test/core/const.wast:681
assert_return(() => invoke($218, `f`, []), [value('f32', 0.000000000000000000000000000000000000000000001)]);

// ./test/core/const.wast:682
let $219 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x0.00000200000000000p-126)))`);

// ./test/core/const.wast:683
assert_return(() => invoke($219, `f`, []), [value('f32', -0.000000000000000000000000000000000000000000001)]);

// ./test/core/const.wast:684
let $220 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x0.00000200000000001p-126)))`);

// ./test/core/const.wast:685
assert_return(() => invoke($220, `f`, []), [value('f32', 0.000000000000000000000000000000000000000000001)]);

// ./test/core/const.wast:686
let $221 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x0.00000200000000001p-126)))`);

// ./test/core/const.wast:687
assert_return(() => invoke($221, `f`, []), [value('f32', -0.000000000000000000000000000000000000000000001)]);

// ./test/core/const.wast:688
let $222 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x0.000002fffffffffffp-126)))`);

// ./test/core/const.wast:689
assert_return(() => invoke($222, `f`, []), [value('f32', 0.000000000000000000000000000000000000000000001)]);

// ./test/core/const.wast:690
let $223 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x0.000002fffffffffffp-126)))`);

// ./test/core/const.wast:691
assert_return(() => invoke($223, `f`, []), [value('f32', -0.000000000000000000000000000000000000000000001)]);

// ./test/core/const.wast:692
let $224 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x0.00000300000000000p-126)))`);

// ./test/core/const.wast:693
assert_return(() => invoke($224, `f`, []), [value('f32', 0.000000000000000000000000000000000000000000003)]);

// ./test/core/const.wast:694
let $225 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x0.00000300000000000p-126)))`);

// ./test/core/const.wast:695
assert_return(() => invoke($225, `f`, []), [value('f32', -0.000000000000000000000000000000000000000000003)]);

// ./test/core/const.wast:696
let $226 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x0.00000300000000001p-126)))`);

// ./test/core/const.wast:697
assert_return(() => invoke($226, `f`, []), [value('f32', 0.000000000000000000000000000000000000000000003)]);

// ./test/core/const.wast:698
let $227 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x0.00000300000000001p-126)))`);

// ./test/core/const.wast:699
assert_return(() => invoke($227, `f`, []), [value('f32', -0.000000000000000000000000000000000000000000003)]);

// ./test/core/const.wast:700
let $228 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x0.000003fffffffffffp-126)))`);

// ./test/core/const.wast:701
assert_return(() => invoke($228, `f`, []), [value('f32', 0.000000000000000000000000000000000000000000003)]);

// ./test/core/const.wast:702
let $229 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x0.000003fffffffffffp-126)))`);

// ./test/core/const.wast:703
assert_return(() => invoke($229, `f`, []), [value('f32', -0.000000000000000000000000000000000000000000003)]);

// ./test/core/const.wast:704
let $230 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x0.00000400000000000p-126)))`);

// ./test/core/const.wast:705
assert_return(() => invoke($230, `f`, []), [value('f32', 0.000000000000000000000000000000000000000000003)]);

// ./test/core/const.wast:706
let $231 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x0.00000400000000000p-126)))`);

// ./test/core/const.wast:707
assert_return(() => invoke($231, `f`, []), [value('f32', -0.000000000000000000000000000000000000000000003)]);

// ./test/core/const.wast:708
let $232 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x0.00000400000000001p-126)))`);

// ./test/core/const.wast:709
assert_return(() => invoke($232, `f`, []), [value('f32', 0.000000000000000000000000000000000000000000003)]);

// ./test/core/const.wast:710
let $233 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x0.00000400000000001p-126)))`);

// ./test/core/const.wast:711
assert_return(() => invoke($233, `f`, []), [value('f32', -0.000000000000000000000000000000000000000000003)]);

// ./test/core/const.wast:712
let $234 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x0.000004fffffffffffp-126)))`);

// ./test/core/const.wast:713
assert_return(() => invoke($234, `f`, []), [value('f32', 0.000000000000000000000000000000000000000000003)]);

// ./test/core/const.wast:714
let $235 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x0.000004fffffffffffp-126)))`);

// ./test/core/const.wast:715
assert_return(() => invoke($235, `f`, []), [value('f32', -0.000000000000000000000000000000000000000000003)]);

// ./test/core/const.wast:716
let $236 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x0.00000500000000000p-126)))`);

// ./test/core/const.wast:717
assert_return(() => invoke($236, `f`, []), [value('f32', 0.000000000000000000000000000000000000000000003)]);

// ./test/core/const.wast:718
let $237 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x0.00000500000000000p-126)))`);

// ./test/core/const.wast:719
assert_return(() => invoke($237, `f`, []), [value('f32', -0.000000000000000000000000000000000000000000003)]);

// ./test/core/const.wast:720
let $238 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x0.00000500000000001p-126)))`);

// ./test/core/const.wast:721
assert_return(() => invoke($238, `f`, []), [value('f32', 0.000000000000000000000000000000000000000000004)]);

// ./test/core/const.wast:722
let $239 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x0.00000500000000001p-126)))`);

// ./test/core/const.wast:723
assert_return(() => invoke($239, `f`, []), [value('f32', -0.000000000000000000000000000000000000000000004)]);

// ./test/core/const.wast:726
let $240 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.fffffe8p127)))`);

// ./test/core/const.wast:727
assert_return(() => invoke($240, `f`, []), [value('f32', 340282350000000000000000000000000000000)]);

// ./test/core/const.wast:728
let $241 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.fffffe8p127)))`);

// ./test/core/const.wast:729
assert_return(() => invoke($241, `f`, []), [value('f32', -340282350000000000000000000000000000000)]);

// ./test/core/const.wast:730
let $242 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.fffffefffffff8p127)))`);

// ./test/core/const.wast:731
assert_return(() => invoke($242, `f`, []), [value('f32', 340282350000000000000000000000000000000)]);

// ./test/core/const.wast:732
let $243 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.fffffefffffff8p127)))`);

// ./test/core/const.wast:733
assert_return(() => invoke($243, `f`, []), [value('f32', -340282350000000000000000000000000000000)]);

// ./test/core/const.wast:734
let $244 = instantiate(`(module (func (export "f") (result f32) (f32.const +0x1.fffffefffffffffffp127)))`);

// ./test/core/const.wast:735
assert_return(() => invoke($244, `f`, []), [value('f32', 340282350000000000000000000000000000000)]);

// ./test/core/const.wast:736
let $245 = instantiate(`(module (func (export "f") (result f32) (f32.const -0x1.fffffefffffffffffp127)))`);

// ./test/core/const.wast:737
assert_return(() => invoke($245, `f`, []), [value('f32', -340282350000000000000000000000000000000)]);

// ./test/core/const.wast:740
let $246 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x1.000000000000080000000000p-600)))`);

// ./test/core/const.wast:741
assert_return(() => invoke($246, `f`, []), [value('f64', 0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002409919865102884)]);

// ./test/core/const.wast:742
let $247 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x1.000000000000080000000000p-600)))`);

// ./test/core/const.wast:743
assert_return(() => invoke($247, `f`, []), [value('f64', -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002409919865102884)]);

// ./test/core/const.wast:744
let $248 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x1.000000000000080000000001p-600)))`);

// ./test/core/const.wast:745
assert_return(() => invoke($248, `f`, []), [value('f64', 0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000024099198651028847)]);

// ./test/core/const.wast:746
let $249 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x1.000000000000080000000001p-600)))`);

// ./test/core/const.wast:747
assert_return(() => invoke($249, `f`, []), [value('f64', -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000024099198651028847)]);

// ./test/core/const.wast:748
let $250 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x1.0000000000000fffffffffffp-600)))`);

// ./test/core/const.wast:749
assert_return(() => invoke($250, `f`, []), [value('f64', 0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000024099198651028847)]);

// ./test/core/const.wast:750
let $251 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x1.0000000000000fffffffffffp-600)))`);

// ./test/core/const.wast:751
assert_return(() => invoke($251, `f`, []), [value('f64', -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000024099198651028847)]);

// ./test/core/const.wast:752
let $252 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x1.000000000000100000000000p-600)))`);

// ./test/core/const.wast:753
assert_return(() => invoke($252, `f`, []), [value('f64', 0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000024099198651028847)]);

// ./test/core/const.wast:754
let $253 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x1.000000000000100000000000p-600)))`);

// ./test/core/const.wast:755
assert_return(() => invoke($253, `f`, []), [value('f64', -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000024099198651028847)]);

// ./test/core/const.wast:756
let $254 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x1.000000000000100000000001p-600)))`);

// ./test/core/const.wast:757
assert_return(() => invoke($254, `f`, []), [value('f64', 0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000024099198651028847)]);

// ./test/core/const.wast:758
let $255 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x1.000000000000100000000001p-600)))`);

// ./test/core/const.wast:759
assert_return(() => invoke($255, `f`, []), [value('f64', -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000024099198651028847)]);

// ./test/core/const.wast:760
let $256 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x1.00000000000017ffffffffffp-600)))`);

// ./test/core/const.wast:761
assert_return(() => invoke($256, `f`, []), [value('f64', 0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000024099198651028847)]);

// ./test/core/const.wast:762
let $257 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x1.00000000000017ffffffffffp-600)))`);

// ./test/core/const.wast:763
assert_return(() => invoke($257, `f`, []), [value('f64', -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000024099198651028847)]);

// ./test/core/const.wast:764
let $258 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x1.000000000000180000000000p-600)))`);

// ./test/core/const.wast:765
assert_return(() => invoke($258, `f`, []), [value('f64', 0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002409919865102885)]);

// ./test/core/const.wast:766
let $259 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x1.000000000000180000000000p-600)))`);

// ./test/core/const.wast:767
assert_return(() => invoke($259, `f`, []), [value('f64', -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002409919865102885)]);

// ./test/core/const.wast:768
let $260 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x1.000000000000180000000001p-600)))`);

// ./test/core/const.wast:769
assert_return(() => invoke($260, `f`, []), [value('f64', 0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002409919865102885)]);

// ./test/core/const.wast:770
let $261 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x1.000000000000180000000001p-600)))`);

// ./test/core/const.wast:771
assert_return(() => invoke($261, `f`, []), [value('f64', -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002409919865102885)]);

// ./test/core/const.wast:772
let $262 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x1.0000000000001fffffffffffp-600)))`);

// ./test/core/const.wast:773
assert_return(() => invoke($262, `f`, []), [value('f64', 0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002409919865102885)]);

// ./test/core/const.wast:774
let $263 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x1.0000000000001fffffffffffp-600)))`);

// ./test/core/const.wast:775
assert_return(() => invoke($263, `f`, []), [value('f64', -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002409919865102885)]);

// ./test/core/const.wast:776
let $264 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x1.000000000000200000000000p-600)))`);

// ./test/core/const.wast:777
assert_return(() => invoke($264, `f`, []), [value('f64', 0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002409919865102885)]);

// ./test/core/const.wast:778
let $265 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x1.000000000000200000000000p-600)))`);

// ./test/core/const.wast:779
assert_return(() => invoke($265, `f`, []), [value('f64', -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002409919865102885)]);

// ./test/core/const.wast:780
let $266 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x1.000000000000200000000001p-600)))`);

// ./test/core/const.wast:781
assert_return(() => invoke($266, `f`, []), [value('f64', 0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002409919865102885)]);

// ./test/core/const.wast:782
let $267 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x1.000000000000200000000001p-600)))`);

// ./test/core/const.wast:783
assert_return(() => invoke($267, `f`, []), [value('f64', -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002409919865102885)]);

// ./test/core/const.wast:784
let $268 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x1.00000000000027ffffffffffp-600)))`);

// ./test/core/const.wast:785
assert_return(() => invoke($268, `f`, []), [value('f64', 0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002409919865102885)]);

// ./test/core/const.wast:786
let $269 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x1.00000000000027ffffffffffp-600)))`);

// ./test/core/const.wast:787
assert_return(() => invoke($269, `f`, []), [value('f64', -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002409919865102885)]);

// ./test/core/const.wast:788
let $270 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x1.000000000000280000000001p-600)))`);

// ./test/core/const.wast:789
assert_return(() => invoke($270, `f`, []), [value('f64', 0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000024099198651028857)]);

// ./test/core/const.wast:790
let $271 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x1.000000000000280000000001p-600)))`);

// ./test/core/const.wast:791
assert_return(() => invoke($271, `f`, []), [value('f64', -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000024099198651028857)]);

// ./test/core/const.wast:793
let $272 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x8000000.000000400000000000p-627)))`);

// ./test/core/const.wast:794
assert_return(() => invoke($272, `f`, []), [value('f64', 0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002409919865102884)]);

// ./test/core/const.wast:795
let $273 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x8000000.000000400000000000p-627)))`);

// ./test/core/const.wast:796
assert_return(() => invoke($273, `f`, []), [value('f64', -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002409919865102884)]);

// ./test/core/const.wast:797
let $274 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x8000000.000000400000000001p-627)))`);

// ./test/core/const.wast:798
assert_return(() => invoke($274, `f`, []), [value('f64', 0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000024099198651028847)]);

// ./test/core/const.wast:799
let $275 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x8000000.000000400000000001p-627)))`);

// ./test/core/const.wast:800
assert_return(() => invoke($275, `f`, []), [value('f64', -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000024099198651028847)]);

// ./test/core/const.wast:801
let $276 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x8000000.0000007fffffffffffp-627)))`);

// ./test/core/const.wast:802
assert_return(() => invoke($276, `f`, []), [value('f64', 0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000024099198651028847)]);

// ./test/core/const.wast:803
let $277 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x8000000.0000007fffffffffffp-627)))`);

// ./test/core/const.wast:804
assert_return(() => invoke($277, `f`, []), [value('f64', -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000024099198651028847)]);

// ./test/core/const.wast:805
let $278 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x8000000.000000800000000000p-627)))`);

// ./test/core/const.wast:806
assert_return(() => invoke($278, `f`, []), [value('f64', 0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000024099198651028847)]);

// ./test/core/const.wast:807
let $279 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x8000000.000000800000000000p-627)))`);

// ./test/core/const.wast:808
assert_return(() => invoke($279, `f`, []), [value('f64', -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000024099198651028847)]);

// ./test/core/const.wast:809
let $280 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x8000000.000000800000000001p-627)))`);

// ./test/core/const.wast:810
assert_return(() => invoke($280, `f`, []), [value('f64', 0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000024099198651028847)]);

// ./test/core/const.wast:811
let $281 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x8000000.000000800000000001p-627)))`);

// ./test/core/const.wast:812
assert_return(() => invoke($281, `f`, []), [value('f64', -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000024099198651028847)]);

// ./test/core/const.wast:813
let $282 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x8000000.000000bfffffffffffp-627)))`);

// ./test/core/const.wast:814
assert_return(() => invoke($282, `f`, []), [value('f64', 0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000024099198651028847)]);

// ./test/core/const.wast:815
let $283 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x8000000.000000bfffffffffffp-627)))`);

// ./test/core/const.wast:816
assert_return(() => invoke($283, `f`, []), [value('f64', -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000024099198651028847)]);

// ./test/core/const.wast:817
let $284 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x8000000.000000c00000000000p-627)))`);

// ./test/core/const.wast:818
assert_return(() => invoke($284, `f`, []), [value('f64', 0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002409919865102885)]);

// ./test/core/const.wast:819
let $285 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x8000000.000000c00000000000p-627)))`);

// ./test/core/const.wast:820
assert_return(() => invoke($285, `f`, []), [value('f64', -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002409919865102885)]);

// ./test/core/const.wast:821
let $286 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x8000000.000000c00000000001p-627)))`);

// ./test/core/const.wast:822
assert_return(() => invoke($286, `f`, []), [value('f64', 0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002409919865102885)]);

// ./test/core/const.wast:823
let $287 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x8000000.000000c00000000001p-627)))`);

// ./test/core/const.wast:824
assert_return(() => invoke($287, `f`, []), [value('f64', -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002409919865102885)]);

// ./test/core/const.wast:825
let $288 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x8000000.000000ffffffffffffp-627)))`);

// ./test/core/const.wast:826
assert_return(() => invoke($288, `f`, []), [value('f64', 0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002409919865102885)]);

// ./test/core/const.wast:827
let $289 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x8000000.000000ffffffffffffp-627)))`);

// ./test/core/const.wast:828
assert_return(() => invoke($289, `f`, []), [value('f64', -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002409919865102885)]);

// ./test/core/const.wast:829
let $290 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x8000000.000001000000000000p-627)))`);

// ./test/core/const.wast:830
assert_return(() => invoke($290, `f`, []), [value('f64', 0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002409919865102885)]);

// ./test/core/const.wast:831
let $291 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x8000000.000001000000000000p-627)))`);

// ./test/core/const.wast:832
assert_return(() => invoke($291, `f`, []), [value('f64', -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002409919865102885)]);

// ./test/core/const.wast:833
let $292 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x8000000.000001000000000001p-627)))`);

// ./test/core/const.wast:834
assert_return(() => invoke($292, `f`, []), [value('f64', 0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002409919865102885)]);

// ./test/core/const.wast:835
let $293 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x8000000.000001000000000001p-627)))`);

// ./test/core/const.wast:836
assert_return(() => invoke($293, `f`, []), [value('f64', -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002409919865102885)]);

// ./test/core/const.wast:837
let $294 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x8000000.0000013fffffffffffp-627)))`);

// ./test/core/const.wast:838
assert_return(() => invoke($294, `f`, []), [value('f64', 0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002409919865102885)]);

// ./test/core/const.wast:839
let $295 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x8000000.0000013fffffffffffp-627)))`);

// ./test/core/const.wast:840
assert_return(() => invoke($295, `f`, []), [value('f64', -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002409919865102885)]);

// ./test/core/const.wast:841
let $296 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x8000000.000001400000000001p-627)))`);

// ./test/core/const.wast:842
assert_return(() => invoke($296, `f`, []), [value('f64', 0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000024099198651028857)]);

// ./test/core/const.wast:843
let $297 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x8000000.000001400000000001p-627)))`);

// ./test/core/const.wast:844
assert_return(() => invoke($297, `f`, []), [value('f64', -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000024099198651028857)]);

// ./test/core/const.wast:846
let $298 = instantiate(`(module (func (export "f") (result f64) (f64.const +5.3575430359313371995e+300)))`);

// ./test/core/const.wast:847
assert_return(() => invoke($298, `f`, []), [value('f64', 5357543035931337000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:848
let $299 = instantiate(`(module (func (export "f") (result f64) (f64.const -5.3575430359313371995e+300)))`);

// ./test/core/const.wast:849
assert_return(() => invoke($299, `f`, []), [value('f64', -5357543035931337000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:850
let $300 = instantiate(`(module (func (export "f") (result f64) (f64.const +5.3575430359313371996e+300)))`);

// ./test/core/const.wast:851
assert_return(() => invoke($300, `f`, []), [value('f64', 5357543035931338000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:852
let $301 = instantiate(`(module (func (export "f") (result f64) (f64.const -5.3575430359313371996e+300)))`);

// ./test/core/const.wast:853
assert_return(() => invoke($301, `f`, []), [value('f64', -5357543035931338000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:854
let $302 = instantiate(`(module (func (export "f") (result f64) (f64.const +5.3575430359313383891e+300)))`);

// ./test/core/const.wast:855
assert_return(() => invoke($302, `f`, []), [value('f64', 5357543035931338000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:856
let $303 = instantiate(`(module (func (export "f") (result f64) (f64.const -5.3575430359313383891e+300)))`);

// ./test/core/const.wast:857
assert_return(() => invoke($303, `f`, []), [value('f64', -5357543035931338000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:858
let $304 = instantiate(`(module (func (export "f") (result f64) (f64.const +5.3575430359313383892e+300)))`);

// ./test/core/const.wast:859
assert_return(() => invoke($304, `f`, []), [value('f64', 5357543035931339000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:860
let $305 = instantiate(`(module (func (export "f") (result f64) (f64.const -5.3575430359313383892e+300)))`);

// ./test/core/const.wast:861
assert_return(() => invoke($305, `f`, []), [value('f64', -5357543035931339000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:864
let $306 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x1.000000000000080000000000p+600)))`);

// ./test/core/const.wast:865
assert_return(() => invoke($306, `f`, []), [value('f64', 4149515568880993000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:866
let $307 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x1.000000000000080000000000p+600)))`);

// ./test/core/const.wast:867
assert_return(() => invoke($307, `f`, []), [value('f64', -4149515568880993000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:868
let $308 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x1.000000000000080000000001p+600)))`);

// ./test/core/const.wast:869
assert_return(() => invoke($308, `f`, []), [value('f64', 4149515568880994000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:870
let $309 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x1.000000000000080000000001p+600)))`);

// ./test/core/const.wast:871
assert_return(() => invoke($309, `f`, []), [value('f64', -4149515568880994000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:872
let $310 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x1.0000000000000fffffffffffp+600)))`);

// ./test/core/const.wast:873
assert_return(() => invoke($310, `f`, []), [value('f64', 4149515568880994000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:874
let $311 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x1.0000000000000fffffffffffp+600)))`);

// ./test/core/const.wast:875
assert_return(() => invoke($311, `f`, []), [value('f64', -4149515568880994000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:876
let $312 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x1.000000000000100000000000p+600)))`);

// ./test/core/const.wast:877
assert_return(() => invoke($312, `f`, []), [value('f64', 4149515568880994000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:878
let $313 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x1.000000000000100000000000p+600)))`);

// ./test/core/const.wast:879
assert_return(() => invoke($313, `f`, []), [value('f64', -4149515568880994000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:880
let $314 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x1.000000000000100000000001p+600)))`);

// ./test/core/const.wast:881
assert_return(() => invoke($314, `f`, []), [value('f64', 4149515568880994000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:882
let $315 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x1.000000000000100000000001p+600)))`);

// ./test/core/const.wast:883
assert_return(() => invoke($315, `f`, []), [value('f64', -4149515568880994000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:884
let $316 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x1.00000000000017ffffffffffp+600)))`);

// ./test/core/const.wast:885
assert_return(() => invoke($316, `f`, []), [value('f64', 4149515568880994000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:886
let $317 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x1.00000000000017ffffffffffp+600)))`);

// ./test/core/const.wast:887
assert_return(() => invoke($317, `f`, []), [value('f64', -4149515568880994000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:888
let $318 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x1.000000000000180000000000p+600)))`);

// ./test/core/const.wast:889
assert_return(() => invoke($318, `f`, []), [value('f64', 4149515568880995000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:890
let $319 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x1.000000000000180000000000p+600)))`);

// ./test/core/const.wast:891
assert_return(() => invoke($319, `f`, []), [value('f64', -4149515568880995000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:892
let $320 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x1.000000000000180000000001p+600)))`);

// ./test/core/const.wast:893
assert_return(() => invoke($320, `f`, []), [value('f64', 4149515568880995000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:894
let $321 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x1.000000000000180000000001p+600)))`);

// ./test/core/const.wast:895
assert_return(() => invoke($321, `f`, []), [value('f64', -4149515568880995000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:896
let $322 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x1.0000000000001fffffffffffp+600)))`);

// ./test/core/const.wast:897
assert_return(() => invoke($322, `f`, []), [value('f64', 4149515568880995000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:898
let $323 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x1.0000000000001fffffffffffp+600)))`);

// ./test/core/const.wast:899
assert_return(() => invoke($323, `f`, []), [value('f64', -4149515568880995000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:900
let $324 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x1.000000000000200000000000p+600)))`);

// ./test/core/const.wast:901
assert_return(() => invoke($324, `f`, []), [value('f64', 4149515568880995000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:902
let $325 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x1.000000000000200000000000p+600)))`);

// ./test/core/const.wast:903
assert_return(() => invoke($325, `f`, []), [value('f64', -4149515568880995000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:904
let $326 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x1.000000000000200000000001p+600)))`);

// ./test/core/const.wast:905
assert_return(() => invoke($326, `f`, []), [value('f64', 4149515568880995000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:906
let $327 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x1.000000000000200000000001p+600)))`);

// ./test/core/const.wast:907
assert_return(() => invoke($327, `f`, []), [value('f64', -4149515568880995000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:908
let $328 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x1.00000000000027ffffffffffp+600)))`);

// ./test/core/const.wast:909
assert_return(() => invoke($328, `f`, []), [value('f64', 4149515568880995000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:910
let $329 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x1.00000000000027ffffffffffp+600)))`);

// ./test/core/const.wast:911
assert_return(() => invoke($329, `f`, []), [value('f64', -4149515568880995000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:912
let $330 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x1.000000000000280000000000p+600)))`);

// ./test/core/const.wast:913
assert_return(() => invoke($330, `f`, []), [value('f64', 4149515568880995000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:914
let $331 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x1.000000000000280000000000p+600)))`);

// ./test/core/const.wast:915
assert_return(() => invoke($331, `f`, []), [value('f64', -4149515568880995000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:916
let $332 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x1.000000000000280000000001p+600)))`);

// ./test/core/const.wast:917
assert_return(() => invoke($332, `f`, []), [value('f64', 4149515568880996000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:918
let $333 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x1.000000000000280000000001p+600)))`);

// ./test/core/const.wast:919
assert_return(() => invoke($333, `f`, []), [value('f64', -4149515568880996000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:921
let $334 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x2000000000000100000000000)))`);

// ./test/core/const.wast:922
assert_return(() => invoke($334, `f`, []), [value('f64', 158456325028528680000000000000)]);

// ./test/core/const.wast:923
let $335 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x2000000000000100000000000)))`);

// ./test/core/const.wast:924
assert_return(() => invoke($335, `f`, []), [value('f64', -158456325028528680000000000000)]);

// ./test/core/const.wast:925
let $336 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x2000000000000100000000001)))`);

// ./test/core/const.wast:926
assert_return(() => invoke($336, `f`, []), [value('f64', 158456325028528700000000000000)]);

// ./test/core/const.wast:927
let $337 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x2000000000000100000000001)))`);

// ./test/core/const.wast:928
assert_return(() => invoke($337, `f`, []), [value('f64', -158456325028528700000000000000)]);

// ./test/core/const.wast:929
let $338 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x20000000000001fffffffffff)))`);

// ./test/core/const.wast:930
assert_return(() => invoke($338, `f`, []), [value('f64', 158456325028528700000000000000)]);

// ./test/core/const.wast:931
let $339 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x20000000000001fffffffffff)))`);

// ./test/core/const.wast:932
assert_return(() => invoke($339, `f`, []), [value('f64', -158456325028528700000000000000)]);

// ./test/core/const.wast:933
let $340 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x2000000000000200000000000)))`);

// ./test/core/const.wast:934
assert_return(() => invoke($340, `f`, []), [value('f64', 158456325028528700000000000000)]);

// ./test/core/const.wast:935
let $341 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x2000000000000200000000000)))`);

// ./test/core/const.wast:936
assert_return(() => invoke($341, `f`, []), [value('f64', -158456325028528700000000000000)]);

// ./test/core/const.wast:937
let $342 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x2000000000000200000000001)))`);

// ./test/core/const.wast:938
assert_return(() => invoke($342, `f`, []), [value('f64', 158456325028528700000000000000)]);

// ./test/core/const.wast:939
let $343 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x2000000000000200000000001)))`);

// ./test/core/const.wast:940
assert_return(() => invoke($343, `f`, []), [value('f64', -158456325028528700000000000000)]);

// ./test/core/const.wast:941
let $344 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x20000000000002fffffffffff)))`);

// ./test/core/const.wast:942
assert_return(() => invoke($344, `f`, []), [value('f64', 158456325028528700000000000000)]);

// ./test/core/const.wast:943
let $345 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x20000000000002fffffffffff)))`);

// ./test/core/const.wast:944
assert_return(() => invoke($345, `f`, []), [value('f64', -158456325028528700000000000000)]);

// ./test/core/const.wast:945
let $346 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x2000000000000300000000000)))`);

// ./test/core/const.wast:946
assert_return(() => invoke($346, `f`, []), [value('f64', 158456325028528750000000000000)]);

// ./test/core/const.wast:947
let $347 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x2000000000000300000000000)))`);

// ./test/core/const.wast:948
assert_return(() => invoke($347, `f`, []), [value('f64', -158456325028528750000000000000)]);

// ./test/core/const.wast:949
let $348 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x2000000000000300000000001)))`);

// ./test/core/const.wast:950
assert_return(() => invoke($348, `f`, []), [value('f64', 158456325028528750000000000000)]);

// ./test/core/const.wast:951
let $349 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x2000000000000300000000001)))`);

// ./test/core/const.wast:952
assert_return(() => invoke($349, `f`, []), [value('f64', -158456325028528750000000000000)]);

// ./test/core/const.wast:953
let $350 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x20000000000003fffffffffff)))`);

// ./test/core/const.wast:954
assert_return(() => invoke($350, `f`, []), [value('f64', 158456325028528750000000000000)]);

// ./test/core/const.wast:955
let $351 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x20000000000003fffffffffff)))`);

// ./test/core/const.wast:956
assert_return(() => invoke($351, `f`, []), [value('f64', -158456325028528750000000000000)]);

// ./test/core/const.wast:957
let $352 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x2000000000000400000000000)))`);

// ./test/core/const.wast:958
assert_return(() => invoke($352, `f`, []), [value('f64', 158456325028528750000000000000)]);

// ./test/core/const.wast:959
let $353 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x2000000000000400000000000)))`);

// ./test/core/const.wast:960
assert_return(() => invoke($353, `f`, []), [value('f64', -158456325028528750000000000000)]);

// ./test/core/const.wast:961
let $354 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x2000000000000400000000001)))`);

// ./test/core/const.wast:962
assert_return(() => invoke($354, `f`, []), [value('f64', 158456325028528750000000000000)]);

// ./test/core/const.wast:963
let $355 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x2000000000000400000000001)))`);

// ./test/core/const.wast:964
assert_return(() => invoke($355, `f`, []), [value('f64', -158456325028528750000000000000)]);

// ./test/core/const.wast:965
let $356 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x20000000000004fffffffffff)))`);

// ./test/core/const.wast:966
assert_return(() => invoke($356, `f`, []), [value('f64', 158456325028528750000000000000)]);

// ./test/core/const.wast:967
let $357 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x20000000000004fffffffffff)))`);

// ./test/core/const.wast:968
assert_return(() => invoke($357, `f`, []), [value('f64', -158456325028528750000000000000)]);

// ./test/core/const.wast:969
let $358 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x2000000000000500000000000)))`);

// ./test/core/const.wast:970
assert_return(() => invoke($358, `f`, []), [value('f64', 158456325028528750000000000000)]);

// ./test/core/const.wast:971
let $359 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x2000000000000500000000000)))`);

// ./test/core/const.wast:972
assert_return(() => invoke($359, `f`, []), [value('f64', -158456325028528750000000000000)]);

// ./test/core/const.wast:973
let $360 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x2000000000000500000000001)))`);

// ./test/core/const.wast:974
assert_return(() => invoke($360, `f`, []), [value('f64', 158456325028528780000000000000)]);

// ./test/core/const.wast:975
let $361 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x2000000000000500000000001)))`);

// ./test/core/const.wast:976
assert_return(() => invoke($361, `f`, []), [value('f64', -158456325028528780000000000000)]);

// ./test/core/const.wast:978
let $362 = instantiate(`(module (func (export "f") (result f64) (f64.const +1152921504606847104)))`);

// ./test/core/const.wast:979
assert_return(() => invoke($362, `f`, []), [value('f64', 1152921504606847000)]);

// ./test/core/const.wast:980
let $363 = instantiate(`(module (func (export "f") (result f64) (f64.const -1152921504606847104)))`);

// ./test/core/const.wast:981
assert_return(() => invoke($363, `f`, []), [value('f64', -1152921504606847000)]);

// ./test/core/const.wast:982
let $364 = instantiate(`(module (func (export "f") (result f64) (f64.const +1152921504606847105)))`);

// ./test/core/const.wast:983
assert_return(() => invoke($364, `f`, []), [value('f64', 1152921504606847200)]);

// ./test/core/const.wast:984
let $365 = instantiate(`(module (func (export "f") (result f64) (f64.const -1152921504606847105)))`);

// ./test/core/const.wast:985
assert_return(() => invoke($365, `f`, []), [value('f64', -1152921504606847200)]);

// ./test/core/const.wast:986
let $366 = instantiate(`(module (func (export "f") (result f64) (f64.const +1152921504606847359)))`);

// ./test/core/const.wast:987
assert_return(() => invoke($366, `f`, []), [value('f64', 1152921504606847200)]);

// ./test/core/const.wast:988
let $367 = instantiate(`(module (func (export "f") (result f64) (f64.const -1152921504606847359)))`);

// ./test/core/const.wast:989
assert_return(() => invoke($367, `f`, []), [value('f64', -1152921504606847200)]);

// ./test/core/const.wast:990
let $368 = instantiate(`(module (func (export "f") (result f64) (f64.const +1152921504606847360)))`);

// ./test/core/const.wast:991
assert_return(() => invoke($368, `f`, []), [value('f64', 1152921504606847500)]);

// ./test/core/const.wast:992
let $369 = instantiate(`(module (func (export "f") (result f64) (f64.const -1152921504606847360)))`);

// ./test/core/const.wast:993
assert_return(() => invoke($369, `f`, []), [value('f64', -1152921504606847500)]);

// ./test/core/const.wast:996
let $370 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x0.000000000000080000000000p-1022)))`);

// ./test/core/const.wast:997
assert_return(() => invoke($370, `f`, []), [value('f64', 0)]);

// ./test/core/const.wast:998
let $371 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x0.000000000000080000000000p-1022)))`);

// ./test/core/const.wast:999
assert_return(() => invoke($371, `f`, []), [value('f64', -0)]);

// ./test/core/const.wast:1000
let $372 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x0.000000000000080000000001p-1022)))`);

// ./test/core/const.wast:1001
assert_return(() => invoke($372, `f`, []), [value('f64', 0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005)]);

// ./test/core/const.wast:1002
let $373 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x0.000000000000080000000001p-1022)))`);

// ./test/core/const.wast:1003
assert_return(() => invoke($373, `f`, []), [value('f64', -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005)]);

// ./test/core/const.wast:1004
let $374 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x0.0000000000000fffffffffffp-1022)))`);

// ./test/core/const.wast:1005
assert_return(() => invoke($374, `f`, []), [value('f64', 0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005)]);

// ./test/core/const.wast:1006
let $375 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x0.0000000000000fffffffffffp-1022)))`);

// ./test/core/const.wast:1007
assert_return(() => invoke($375, `f`, []), [value('f64', -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005)]);

// ./test/core/const.wast:1008
let $376 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x0.000000000000100000000000p-1022)))`);

// ./test/core/const.wast:1009
assert_return(() => invoke($376, `f`, []), [value('f64', 0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005)]);

// ./test/core/const.wast:1010
let $377 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x0.000000000000100000000000p-1022)))`);

// ./test/core/const.wast:1011
assert_return(() => invoke($377, `f`, []), [value('f64', -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005)]);

// ./test/core/const.wast:1012
let $378 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x0.000000000000100000000001p-1022)))`);

// ./test/core/const.wast:1013
assert_return(() => invoke($378, `f`, []), [value('f64', 0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005)]);

// ./test/core/const.wast:1014
let $379 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x0.000000000000100000000001p-1022)))`);

// ./test/core/const.wast:1015
assert_return(() => invoke($379, `f`, []), [value('f64', -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005)]);

// ./test/core/const.wast:1016
let $380 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x0.00000000000017ffffffffffp-1022)))`);

// ./test/core/const.wast:1017
assert_return(() => invoke($380, `f`, []), [value('f64', 0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005)]);

// ./test/core/const.wast:1018
let $381 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x0.00000000000017ffffffffffp-1022)))`);

// ./test/core/const.wast:1019
assert_return(() => invoke($381, `f`, []), [value('f64', -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005)]);

// ./test/core/const.wast:1020
let $382 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x0.000000000000180000000000p-1022)))`);

// ./test/core/const.wast:1021
assert_return(() => invoke($382, `f`, []), [value('f64', 0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001)]);

// ./test/core/const.wast:1022
let $383 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x0.000000000000180000000000p-1022)))`);

// ./test/core/const.wast:1023
assert_return(() => invoke($383, `f`, []), [value('f64', -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001)]);

// ./test/core/const.wast:1024
let $384 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x0.000000000000180000000001p-1022)))`);

// ./test/core/const.wast:1025
assert_return(() => invoke($384, `f`, []), [value('f64', 0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001)]);

// ./test/core/const.wast:1026
let $385 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x0.000000000000180000000001p-1022)))`);

// ./test/core/const.wast:1027
assert_return(() => invoke($385, `f`, []), [value('f64', -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001)]);

// ./test/core/const.wast:1028
let $386 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x0.0000000000001fffffffffffp-1022)))`);

// ./test/core/const.wast:1029
assert_return(() => invoke($386, `f`, []), [value('f64', 0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001)]);

// ./test/core/const.wast:1030
let $387 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x0.0000000000001fffffffffffp-1022)))`);

// ./test/core/const.wast:1031
assert_return(() => invoke($387, `f`, []), [value('f64', -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001)]);

// ./test/core/const.wast:1032
let $388 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x0.000000000000200000000000p-1022)))`);

// ./test/core/const.wast:1033
assert_return(() => invoke($388, `f`, []), [value('f64', 0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001)]);

// ./test/core/const.wast:1034
let $389 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x0.000000000000200000000000p-1022)))`);

// ./test/core/const.wast:1035
assert_return(() => invoke($389, `f`, []), [value('f64', -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001)]);

// ./test/core/const.wast:1036
let $390 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x0.000000000000200000000001p-1022)))`);

// ./test/core/const.wast:1037
assert_return(() => invoke($390, `f`, []), [value('f64', 0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001)]);

// ./test/core/const.wast:1038
let $391 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x0.000000000000200000000001p-1022)))`);

// ./test/core/const.wast:1039
assert_return(() => invoke($391, `f`, []), [value('f64', -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001)]);

// ./test/core/const.wast:1040
let $392 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x0.00000000000027ffffffffffp-1022)))`);

// ./test/core/const.wast:1041
assert_return(() => invoke($392, `f`, []), [value('f64', 0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001)]);

// ./test/core/const.wast:1042
let $393 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x0.00000000000027ffffffffffp-1022)))`);

// ./test/core/const.wast:1043
assert_return(() => invoke($393, `f`, []), [value('f64', -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001)]);

// ./test/core/const.wast:1044
let $394 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x0.000000000000280000000000p-1022)))`);

// ./test/core/const.wast:1045
assert_return(() => invoke($394, `f`, []), [value('f64', 0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001)]);

// ./test/core/const.wast:1046
let $395 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x0.000000000000280000000000p-1022)))`);

// ./test/core/const.wast:1047
assert_return(() => invoke($395, `f`, []), [value('f64', -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001)]);

// ./test/core/const.wast:1048
let $396 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x1.000000000000280000000001p-1022)))`);

// ./test/core/const.wast:1049
assert_return(() => invoke($396, `f`, []), [value('f64', 0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002225073858507203)]);

// ./test/core/const.wast:1050
let $397 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x1.000000000000280000000001p-1022)))`);

// ./test/core/const.wast:1051
assert_return(() => invoke($397, `f`, []), [value('f64', -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002225073858507203)]);

// ./test/core/const.wast:1054
let $398 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x1.fffffffffffff4p1023)))`);

// ./test/core/const.wast:1055
assert_return(() => invoke($398, `f`, []), [value('f64', 179769313486231570000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:1056
let $399 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x1.fffffffffffff4p1023)))`);

// ./test/core/const.wast:1057
assert_return(() => invoke($399, `f`, []), [value('f64', -179769313486231570000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:1058
let $400 = instantiate(`(module (func (export "f") (result f64) (f64.const +0x1.fffffffffffff7ffffffp1023)))`);

// ./test/core/const.wast:1059
assert_return(() => invoke($400, `f`, []), [value('f64', 179769313486231570000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/const.wast:1060
let $401 = instantiate(`(module (func (export "f") (result f64) (f64.const -0x1.fffffffffffff7ffffffp1023)))`);

// ./test/core/const.wast:1061
assert_return(() => invoke($401, `f`, []), [value('f64', -179769313486231570000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

