"use strict";
/* Copyright 2016 Mozilla Foundation
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
Object.defineProperty(exports, "__esModule", { value: true });
exports.bytesToString = exports.BinaryReader = exports.Int64 = exports.ElementMode = exports.DataMode = exports.BinaryReaderState = exports.NameType = exports.LinkingType = exports.RelocType = exports.Type = exports.TypeKind = exports.ExternalKind = exports.OperatorCodeNames = exports.OperatorCode = exports.SectionCode = void 0;
// See https://github.com/WebAssembly/design/blob/master/BinaryEncoding.md
var WASM_MAGIC_NUMBER = 0x6d736100;
var WASM_SUPPORTED_EXPERIMENTAL_VERSION = 0xd;
var WASM_SUPPORTED_VERSION = 0x1;
var SectionCode;
(function (SectionCode) {
    SectionCode[SectionCode["Unknown"] = -1] = "Unknown";
    SectionCode[SectionCode["Custom"] = 0] = "Custom";
    SectionCode[SectionCode["Type"] = 1] = "Type";
    SectionCode[SectionCode["Import"] = 2] = "Import";
    SectionCode[SectionCode["Function"] = 3] = "Function";
    SectionCode[SectionCode["Table"] = 4] = "Table";
    SectionCode[SectionCode["Memory"] = 5] = "Memory";
    SectionCode[SectionCode["Global"] = 6] = "Global";
    SectionCode[SectionCode["Export"] = 7] = "Export";
    SectionCode[SectionCode["Start"] = 8] = "Start";
    SectionCode[SectionCode["Element"] = 9] = "Element";
    SectionCode[SectionCode["Code"] = 10] = "Code";
    SectionCode[SectionCode["Data"] = 11] = "Data";
    SectionCode[SectionCode["Event"] = 13] = "Event";
})(SectionCode = exports.SectionCode || (exports.SectionCode = {}));
var OperatorCode;
(function (OperatorCode) {
    OperatorCode[OperatorCode["unreachable"] = 0] = "unreachable";
    OperatorCode[OperatorCode["nop"] = 1] = "nop";
    OperatorCode[OperatorCode["block"] = 2] = "block";
    OperatorCode[OperatorCode["loop"] = 3] = "loop";
    OperatorCode[OperatorCode["if"] = 4] = "if";
    OperatorCode[OperatorCode["else"] = 5] = "else";
    OperatorCode[OperatorCode["try"] = 6] = "try";
    OperatorCode[OperatorCode["catch"] = 7] = "catch";
    OperatorCode[OperatorCode["throw"] = 8] = "throw";
    OperatorCode[OperatorCode["rethrow"] = 9] = "rethrow";
    OperatorCode[OperatorCode["unwind"] = 10] = "unwind";
    OperatorCode[OperatorCode["end"] = 11] = "end";
    OperatorCode[OperatorCode["br"] = 12] = "br";
    OperatorCode[OperatorCode["br_if"] = 13] = "br_if";
    OperatorCode[OperatorCode["br_table"] = 14] = "br_table";
    OperatorCode[OperatorCode["return"] = 15] = "return";
    OperatorCode[OperatorCode["call"] = 16] = "call";
    OperatorCode[OperatorCode["call_indirect"] = 17] = "call_indirect";
    OperatorCode[OperatorCode["return_call"] = 18] = "return_call";
    OperatorCode[OperatorCode["return_call_indirect"] = 19] = "return_call_indirect";
    OperatorCode[OperatorCode["call_ref"] = 20] = "call_ref";
    OperatorCode[OperatorCode["return_call_ref"] = 21] = "return_call_ref";
    OperatorCode[OperatorCode["let"] = 23] = "let";
    OperatorCode[OperatorCode["delegate"] = 24] = "delegate";
    OperatorCode[OperatorCode["catch_all"] = 25] = "catch_all";
    OperatorCode[OperatorCode["drop"] = 26] = "drop";
    OperatorCode[OperatorCode["select"] = 27] = "select";
    OperatorCode[OperatorCode["local_get"] = 32] = "local_get";
    OperatorCode[OperatorCode["local_set"] = 33] = "local_set";
    OperatorCode[OperatorCode["local_tee"] = 34] = "local_tee";
    OperatorCode[OperatorCode["global_get"] = 35] = "global_get";
    OperatorCode[OperatorCode["global_set"] = 36] = "global_set";
    OperatorCode[OperatorCode["i32_load"] = 40] = "i32_load";
    OperatorCode[OperatorCode["i64_load"] = 41] = "i64_load";
    OperatorCode[OperatorCode["f32_load"] = 42] = "f32_load";
    OperatorCode[OperatorCode["f64_load"] = 43] = "f64_load";
    OperatorCode[OperatorCode["i32_load8_s"] = 44] = "i32_load8_s";
    OperatorCode[OperatorCode["i32_load8_u"] = 45] = "i32_load8_u";
    OperatorCode[OperatorCode["i32_load16_s"] = 46] = "i32_load16_s";
    OperatorCode[OperatorCode["i32_load16_u"] = 47] = "i32_load16_u";
    OperatorCode[OperatorCode["i64_load8_s"] = 48] = "i64_load8_s";
    OperatorCode[OperatorCode["i64_load8_u"] = 49] = "i64_load8_u";
    OperatorCode[OperatorCode["i64_load16_s"] = 50] = "i64_load16_s";
    OperatorCode[OperatorCode["i64_load16_u"] = 51] = "i64_load16_u";
    OperatorCode[OperatorCode["i64_load32_s"] = 52] = "i64_load32_s";
    OperatorCode[OperatorCode["i64_load32_u"] = 53] = "i64_load32_u";
    OperatorCode[OperatorCode["i32_store"] = 54] = "i32_store";
    OperatorCode[OperatorCode["i64_store"] = 55] = "i64_store";
    OperatorCode[OperatorCode["f32_store"] = 56] = "f32_store";
    OperatorCode[OperatorCode["f64_store"] = 57] = "f64_store";
    OperatorCode[OperatorCode["i32_store8"] = 58] = "i32_store8";
    OperatorCode[OperatorCode["i32_store16"] = 59] = "i32_store16";
    OperatorCode[OperatorCode["i64_store8"] = 60] = "i64_store8";
    OperatorCode[OperatorCode["i64_store16"] = 61] = "i64_store16";
    OperatorCode[OperatorCode["i64_store32"] = 62] = "i64_store32";
    OperatorCode[OperatorCode["current_memory"] = 63] = "current_memory";
    OperatorCode[OperatorCode["grow_memory"] = 64] = "grow_memory";
    OperatorCode[OperatorCode["i32_const"] = 65] = "i32_const";
    OperatorCode[OperatorCode["i64_const"] = 66] = "i64_const";
    OperatorCode[OperatorCode["f32_const"] = 67] = "f32_const";
    OperatorCode[OperatorCode["f64_const"] = 68] = "f64_const";
    OperatorCode[OperatorCode["i32_eqz"] = 69] = "i32_eqz";
    OperatorCode[OperatorCode["i32_eq"] = 70] = "i32_eq";
    OperatorCode[OperatorCode["i32_ne"] = 71] = "i32_ne";
    OperatorCode[OperatorCode["i32_lt_s"] = 72] = "i32_lt_s";
    OperatorCode[OperatorCode["i32_lt_u"] = 73] = "i32_lt_u";
    OperatorCode[OperatorCode["i32_gt_s"] = 74] = "i32_gt_s";
    OperatorCode[OperatorCode["i32_gt_u"] = 75] = "i32_gt_u";
    OperatorCode[OperatorCode["i32_le_s"] = 76] = "i32_le_s";
    OperatorCode[OperatorCode["i32_le_u"] = 77] = "i32_le_u";
    OperatorCode[OperatorCode["i32_ge_s"] = 78] = "i32_ge_s";
    OperatorCode[OperatorCode["i32_ge_u"] = 79] = "i32_ge_u";
    OperatorCode[OperatorCode["i64_eqz"] = 80] = "i64_eqz";
    OperatorCode[OperatorCode["i64_eq"] = 81] = "i64_eq";
    OperatorCode[OperatorCode["i64_ne"] = 82] = "i64_ne";
    OperatorCode[OperatorCode["i64_lt_s"] = 83] = "i64_lt_s";
    OperatorCode[OperatorCode["i64_lt_u"] = 84] = "i64_lt_u";
    OperatorCode[OperatorCode["i64_gt_s"] = 85] = "i64_gt_s";
    OperatorCode[OperatorCode["i64_gt_u"] = 86] = "i64_gt_u";
    OperatorCode[OperatorCode["i64_le_s"] = 87] = "i64_le_s";
    OperatorCode[OperatorCode["i64_le_u"] = 88] = "i64_le_u";
    OperatorCode[OperatorCode["i64_ge_s"] = 89] = "i64_ge_s";
    OperatorCode[OperatorCode["i64_ge_u"] = 90] = "i64_ge_u";
    OperatorCode[OperatorCode["f32_eq"] = 91] = "f32_eq";
    OperatorCode[OperatorCode["f32_ne"] = 92] = "f32_ne";
    OperatorCode[OperatorCode["f32_lt"] = 93] = "f32_lt";
    OperatorCode[OperatorCode["f32_gt"] = 94] = "f32_gt";
    OperatorCode[OperatorCode["f32_le"] = 95] = "f32_le";
    OperatorCode[OperatorCode["f32_ge"] = 96] = "f32_ge";
    OperatorCode[OperatorCode["f64_eq"] = 97] = "f64_eq";
    OperatorCode[OperatorCode["f64_ne"] = 98] = "f64_ne";
    OperatorCode[OperatorCode["f64_lt"] = 99] = "f64_lt";
    OperatorCode[OperatorCode["f64_gt"] = 100] = "f64_gt";
    OperatorCode[OperatorCode["f64_le"] = 101] = "f64_le";
    OperatorCode[OperatorCode["f64_ge"] = 102] = "f64_ge";
    OperatorCode[OperatorCode["i32_clz"] = 103] = "i32_clz";
    OperatorCode[OperatorCode["i32_ctz"] = 104] = "i32_ctz";
    OperatorCode[OperatorCode["i32_popcnt"] = 105] = "i32_popcnt";
    OperatorCode[OperatorCode["i32_add"] = 106] = "i32_add";
    OperatorCode[OperatorCode["i32_sub"] = 107] = "i32_sub";
    OperatorCode[OperatorCode["i32_mul"] = 108] = "i32_mul";
    OperatorCode[OperatorCode["i32_div_s"] = 109] = "i32_div_s";
    OperatorCode[OperatorCode["i32_div_u"] = 110] = "i32_div_u";
    OperatorCode[OperatorCode["i32_rem_s"] = 111] = "i32_rem_s";
    OperatorCode[OperatorCode["i32_rem_u"] = 112] = "i32_rem_u";
    OperatorCode[OperatorCode["i32_and"] = 113] = "i32_and";
    OperatorCode[OperatorCode["i32_or"] = 114] = "i32_or";
    OperatorCode[OperatorCode["i32_xor"] = 115] = "i32_xor";
    OperatorCode[OperatorCode["i32_shl"] = 116] = "i32_shl";
    OperatorCode[OperatorCode["i32_shr_s"] = 117] = "i32_shr_s";
    OperatorCode[OperatorCode["i32_shr_u"] = 118] = "i32_shr_u";
    OperatorCode[OperatorCode["i32_rotl"] = 119] = "i32_rotl";
    OperatorCode[OperatorCode["i32_rotr"] = 120] = "i32_rotr";
    OperatorCode[OperatorCode["i64_clz"] = 121] = "i64_clz";
    OperatorCode[OperatorCode["i64_ctz"] = 122] = "i64_ctz";
    OperatorCode[OperatorCode["i64_popcnt"] = 123] = "i64_popcnt";
    OperatorCode[OperatorCode["i64_add"] = 124] = "i64_add";
    OperatorCode[OperatorCode["i64_sub"] = 125] = "i64_sub";
    OperatorCode[OperatorCode["i64_mul"] = 126] = "i64_mul";
    OperatorCode[OperatorCode["i64_div_s"] = 127] = "i64_div_s";
    OperatorCode[OperatorCode["i64_div_u"] = 128] = "i64_div_u";
    OperatorCode[OperatorCode["i64_rem_s"] = 129] = "i64_rem_s";
    OperatorCode[OperatorCode["i64_rem_u"] = 130] = "i64_rem_u";
    OperatorCode[OperatorCode["i64_and"] = 131] = "i64_and";
    OperatorCode[OperatorCode["i64_or"] = 132] = "i64_or";
    OperatorCode[OperatorCode["i64_xor"] = 133] = "i64_xor";
    OperatorCode[OperatorCode["i64_shl"] = 134] = "i64_shl";
    OperatorCode[OperatorCode["i64_shr_s"] = 135] = "i64_shr_s";
    OperatorCode[OperatorCode["i64_shr_u"] = 136] = "i64_shr_u";
    OperatorCode[OperatorCode["i64_rotl"] = 137] = "i64_rotl";
    OperatorCode[OperatorCode["i64_rotr"] = 138] = "i64_rotr";
    OperatorCode[OperatorCode["f32_abs"] = 139] = "f32_abs";
    OperatorCode[OperatorCode["f32_neg"] = 140] = "f32_neg";
    OperatorCode[OperatorCode["f32_ceil"] = 141] = "f32_ceil";
    OperatorCode[OperatorCode["f32_floor"] = 142] = "f32_floor";
    OperatorCode[OperatorCode["f32_trunc"] = 143] = "f32_trunc";
    OperatorCode[OperatorCode["f32_nearest"] = 144] = "f32_nearest";
    OperatorCode[OperatorCode["f32_sqrt"] = 145] = "f32_sqrt";
    OperatorCode[OperatorCode["f32_add"] = 146] = "f32_add";
    OperatorCode[OperatorCode["f32_sub"] = 147] = "f32_sub";
    OperatorCode[OperatorCode["f32_mul"] = 148] = "f32_mul";
    OperatorCode[OperatorCode["f32_div"] = 149] = "f32_div";
    OperatorCode[OperatorCode["f32_min"] = 150] = "f32_min";
    OperatorCode[OperatorCode["f32_max"] = 151] = "f32_max";
    OperatorCode[OperatorCode["f32_copysign"] = 152] = "f32_copysign";
    OperatorCode[OperatorCode["f64_abs"] = 153] = "f64_abs";
    OperatorCode[OperatorCode["f64_neg"] = 154] = "f64_neg";
    OperatorCode[OperatorCode["f64_ceil"] = 155] = "f64_ceil";
    OperatorCode[OperatorCode["f64_floor"] = 156] = "f64_floor";
    OperatorCode[OperatorCode["f64_trunc"] = 157] = "f64_trunc";
    OperatorCode[OperatorCode["f64_nearest"] = 158] = "f64_nearest";
    OperatorCode[OperatorCode["f64_sqrt"] = 159] = "f64_sqrt";
    OperatorCode[OperatorCode["f64_add"] = 160] = "f64_add";
    OperatorCode[OperatorCode["f64_sub"] = 161] = "f64_sub";
    OperatorCode[OperatorCode["f64_mul"] = 162] = "f64_mul";
    OperatorCode[OperatorCode["f64_div"] = 163] = "f64_div";
    OperatorCode[OperatorCode["f64_min"] = 164] = "f64_min";
    OperatorCode[OperatorCode["f64_max"] = 165] = "f64_max";
    OperatorCode[OperatorCode["f64_copysign"] = 166] = "f64_copysign";
    OperatorCode[OperatorCode["i32_wrap_i64"] = 167] = "i32_wrap_i64";
    OperatorCode[OperatorCode["i32_trunc_f32_s"] = 168] = "i32_trunc_f32_s";
    OperatorCode[OperatorCode["i32_trunc_f32_u"] = 169] = "i32_trunc_f32_u";
    OperatorCode[OperatorCode["i32_trunc_f64_s"] = 170] = "i32_trunc_f64_s";
    OperatorCode[OperatorCode["i32_trunc_f64_u"] = 171] = "i32_trunc_f64_u";
    OperatorCode[OperatorCode["i64_extend_i32_s"] = 172] = "i64_extend_i32_s";
    OperatorCode[OperatorCode["i64_extend_i32_u"] = 173] = "i64_extend_i32_u";
    OperatorCode[OperatorCode["i64_trunc_f32_s"] = 174] = "i64_trunc_f32_s";
    OperatorCode[OperatorCode["i64_trunc_f32_u"] = 175] = "i64_trunc_f32_u";
    OperatorCode[OperatorCode["i64_trunc_f64_s"] = 176] = "i64_trunc_f64_s";
    OperatorCode[OperatorCode["i64_trunc_f64_u"] = 177] = "i64_trunc_f64_u";
    OperatorCode[OperatorCode["f32_convert_i32_s"] = 178] = "f32_convert_i32_s";
    OperatorCode[OperatorCode["f32_convert_i32_u"] = 179] = "f32_convert_i32_u";
    OperatorCode[OperatorCode["f32_convert_i64_s"] = 180] = "f32_convert_i64_s";
    OperatorCode[OperatorCode["f32_convert_i64_u"] = 181] = "f32_convert_i64_u";
    OperatorCode[OperatorCode["f32_demote_f64"] = 182] = "f32_demote_f64";
    OperatorCode[OperatorCode["f64_convert_i32_s"] = 183] = "f64_convert_i32_s";
    OperatorCode[OperatorCode["f64_convert_i32_u"] = 184] = "f64_convert_i32_u";
    OperatorCode[OperatorCode["f64_convert_i64_s"] = 185] = "f64_convert_i64_s";
    OperatorCode[OperatorCode["f64_convert_i64_u"] = 186] = "f64_convert_i64_u";
    OperatorCode[OperatorCode["f64_promote_f32"] = 187] = "f64_promote_f32";
    OperatorCode[OperatorCode["i32_reinterpret_f32"] = 188] = "i32_reinterpret_f32";
    OperatorCode[OperatorCode["i64_reinterpret_f64"] = 189] = "i64_reinterpret_f64";
    OperatorCode[OperatorCode["f32_reinterpret_i32"] = 190] = "f32_reinterpret_i32";
    OperatorCode[OperatorCode["f64_reinterpret_i64"] = 191] = "f64_reinterpret_i64";
    OperatorCode[OperatorCode["i32_extend8_s"] = 192] = "i32_extend8_s";
    OperatorCode[OperatorCode["i32_extend16_s"] = 193] = "i32_extend16_s";
    OperatorCode[OperatorCode["i64_extend8_s"] = 194] = "i64_extend8_s";
    OperatorCode[OperatorCode["i64_extend16_s"] = 195] = "i64_extend16_s";
    OperatorCode[OperatorCode["i64_extend32_s"] = 196] = "i64_extend32_s";
    OperatorCode[OperatorCode["prefix_0xfb"] = 251] = "prefix_0xfb";
    OperatorCode[OperatorCode["prefix_0xfc"] = 252] = "prefix_0xfc";
    OperatorCode[OperatorCode["prefix_0xfd"] = 253] = "prefix_0xfd";
    OperatorCode[OperatorCode["prefix_0xfe"] = 254] = "prefix_0xfe";
    OperatorCode[OperatorCode["i32_trunc_sat_f32_s"] = 64512] = "i32_trunc_sat_f32_s";
    OperatorCode[OperatorCode["i32_trunc_sat_f32_u"] = 64513] = "i32_trunc_sat_f32_u";
    OperatorCode[OperatorCode["i32_trunc_sat_f64_s"] = 64514] = "i32_trunc_sat_f64_s";
    OperatorCode[OperatorCode["i32_trunc_sat_f64_u"] = 64515] = "i32_trunc_sat_f64_u";
    OperatorCode[OperatorCode["i64_trunc_sat_f32_s"] = 64516] = "i64_trunc_sat_f32_s";
    OperatorCode[OperatorCode["i64_trunc_sat_f32_u"] = 64517] = "i64_trunc_sat_f32_u";
    OperatorCode[OperatorCode["i64_trunc_sat_f64_s"] = 64518] = "i64_trunc_sat_f64_s";
    OperatorCode[OperatorCode["i64_trunc_sat_f64_u"] = 64519] = "i64_trunc_sat_f64_u";
    OperatorCode[OperatorCode["memory_init"] = 64520] = "memory_init";
    OperatorCode[OperatorCode["data_drop"] = 64521] = "data_drop";
    OperatorCode[OperatorCode["memory_copy"] = 64522] = "memory_copy";
    OperatorCode[OperatorCode["memory_fill"] = 64523] = "memory_fill";
    OperatorCode[OperatorCode["table_init"] = 64524] = "table_init";
    OperatorCode[OperatorCode["elem_drop"] = 64525] = "elem_drop";
    OperatorCode[OperatorCode["table_copy"] = 64526] = "table_copy";
    OperatorCode[OperatorCode["table_grow"] = 64527] = "table_grow";
    OperatorCode[OperatorCode["table_size"] = 64528] = "table_size";
    OperatorCode[OperatorCode["table_fill"] = 64529] = "table_fill";
    OperatorCode[OperatorCode["table_get"] = 37] = "table_get";
    OperatorCode[OperatorCode["table_set"] = 38] = "table_set";
    OperatorCode[OperatorCode["ref_null"] = 208] = "ref_null";
    OperatorCode[OperatorCode["ref_is_null"] = 209] = "ref_is_null";
    OperatorCode[OperatorCode["ref_func"] = 210] = "ref_func";
    OperatorCode[OperatorCode["ref_as_non_null"] = 211] = "ref_as_non_null";
    OperatorCode[OperatorCode["br_on_null"] = 212] = "br_on_null";
    OperatorCode[OperatorCode["ref_eq"] = 213] = "ref_eq";
    OperatorCode[OperatorCode["atomic_notify"] = 65024] = "atomic_notify";
    OperatorCode[OperatorCode["i32_atomic_wait"] = 65025] = "i32_atomic_wait";
    OperatorCode[OperatorCode["i64_atomic_wait"] = 65026] = "i64_atomic_wait";
    OperatorCode[OperatorCode["atomic_fence"] = 65027] = "atomic_fence";
    OperatorCode[OperatorCode["i32_atomic_load"] = 65040] = "i32_atomic_load";
    OperatorCode[OperatorCode["i64_atomic_load"] = 65041] = "i64_atomic_load";
    OperatorCode[OperatorCode["i32_atomic_load8_u"] = 65042] = "i32_atomic_load8_u";
    OperatorCode[OperatorCode["i32_atomic_load16_u"] = 65043] = "i32_atomic_load16_u";
    OperatorCode[OperatorCode["i64_atomic_load8_u"] = 65044] = "i64_atomic_load8_u";
    OperatorCode[OperatorCode["i64_atomic_load16_u"] = 65045] = "i64_atomic_load16_u";
    OperatorCode[OperatorCode["i64_atomic_load32_u"] = 65046] = "i64_atomic_load32_u";
    OperatorCode[OperatorCode["i32_atomic_store"] = 65047] = "i32_atomic_store";
    OperatorCode[OperatorCode["i64_atomic_store"] = 65048] = "i64_atomic_store";
    OperatorCode[OperatorCode["i32_atomic_store8"] = 65049] = "i32_atomic_store8";
    OperatorCode[OperatorCode["i32_atomic_store16"] = 65050] = "i32_atomic_store16";
    OperatorCode[OperatorCode["i64_atomic_store8"] = 65051] = "i64_atomic_store8";
    OperatorCode[OperatorCode["i64_atomic_store16"] = 65052] = "i64_atomic_store16";
    OperatorCode[OperatorCode["i64_atomic_store32"] = 65053] = "i64_atomic_store32";
    OperatorCode[OperatorCode["i32_atomic_rmw_add"] = 65054] = "i32_atomic_rmw_add";
    OperatorCode[OperatorCode["i64_atomic_rmw_add"] = 65055] = "i64_atomic_rmw_add";
    OperatorCode[OperatorCode["i32_atomic_rmw8_add_u"] = 65056] = "i32_atomic_rmw8_add_u";
    OperatorCode[OperatorCode["i32_atomic_rmw16_add_u"] = 65057] = "i32_atomic_rmw16_add_u";
    OperatorCode[OperatorCode["i64_atomic_rmw8_add_u"] = 65058] = "i64_atomic_rmw8_add_u";
    OperatorCode[OperatorCode["i64_atomic_rmw16_add_u"] = 65059] = "i64_atomic_rmw16_add_u";
    OperatorCode[OperatorCode["i64_atomic_rmw32_add_u"] = 65060] = "i64_atomic_rmw32_add_u";
    OperatorCode[OperatorCode["i32_atomic_rmw_sub"] = 65061] = "i32_atomic_rmw_sub";
    OperatorCode[OperatorCode["i64_atomic_rmw_sub"] = 65062] = "i64_atomic_rmw_sub";
    OperatorCode[OperatorCode["i32_atomic_rmw8_sub_u"] = 65063] = "i32_atomic_rmw8_sub_u";
    OperatorCode[OperatorCode["i32_atomic_rmw16_sub_u"] = 65064] = "i32_atomic_rmw16_sub_u";
    OperatorCode[OperatorCode["i64_atomic_rmw8_sub_u"] = 65065] = "i64_atomic_rmw8_sub_u";
    OperatorCode[OperatorCode["i64_atomic_rmw16_sub_u"] = 65066] = "i64_atomic_rmw16_sub_u";
    OperatorCode[OperatorCode["i64_atomic_rmw32_sub_u"] = 65067] = "i64_atomic_rmw32_sub_u";
    OperatorCode[OperatorCode["i32_atomic_rmw_and"] = 65068] = "i32_atomic_rmw_and";
    OperatorCode[OperatorCode["i64_atomic_rmw_and"] = 65069] = "i64_atomic_rmw_and";
    OperatorCode[OperatorCode["i32_atomic_rmw8_and_u"] = 65070] = "i32_atomic_rmw8_and_u";
    OperatorCode[OperatorCode["i32_atomic_rmw16_and_u"] = 65071] = "i32_atomic_rmw16_and_u";
    OperatorCode[OperatorCode["i64_atomic_rmw8_and_u"] = 65072] = "i64_atomic_rmw8_and_u";
    OperatorCode[OperatorCode["i64_atomic_rmw16_and_u"] = 65073] = "i64_atomic_rmw16_and_u";
    OperatorCode[OperatorCode["i64_atomic_rmw32_and_u"] = 65074] = "i64_atomic_rmw32_and_u";
    OperatorCode[OperatorCode["i32_atomic_rmw_or"] = 65075] = "i32_atomic_rmw_or";
    OperatorCode[OperatorCode["i64_atomic_rmw_or"] = 65076] = "i64_atomic_rmw_or";
    OperatorCode[OperatorCode["i32_atomic_rmw8_or_u"] = 65077] = "i32_atomic_rmw8_or_u";
    OperatorCode[OperatorCode["i32_atomic_rmw16_or_u"] = 65078] = "i32_atomic_rmw16_or_u";
    OperatorCode[OperatorCode["i64_atomic_rmw8_or_u"] = 65079] = "i64_atomic_rmw8_or_u";
    OperatorCode[OperatorCode["i64_atomic_rmw16_or_u"] = 65080] = "i64_atomic_rmw16_or_u";
    OperatorCode[OperatorCode["i64_atomic_rmw32_or_u"] = 65081] = "i64_atomic_rmw32_or_u";
    OperatorCode[OperatorCode["i32_atomic_rmw_xor"] = 65082] = "i32_atomic_rmw_xor";
    OperatorCode[OperatorCode["i64_atomic_rmw_xor"] = 65083] = "i64_atomic_rmw_xor";
    OperatorCode[OperatorCode["i32_atomic_rmw8_xor_u"] = 65084] = "i32_atomic_rmw8_xor_u";
    OperatorCode[OperatorCode["i32_atomic_rmw16_xor_u"] = 65085] = "i32_atomic_rmw16_xor_u";
    OperatorCode[OperatorCode["i64_atomic_rmw8_xor_u"] = 65086] = "i64_atomic_rmw8_xor_u";
    OperatorCode[OperatorCode["i64_atomic_rmw16_xor_u"] = 65087] = "i64_atomic_rmw16_xor_u";
    OperatorCode[OperatorCode["i64_atomic_rmw32_xor_u"] = 65088] = "i64_atomic_rmw32_xor_u";
    OperatorCode[OperatorCode["i32_atomic_rmw_xchg"] = 65089] = "i32_atomic_rmw_xchg";
    OperatorCode[OperatorCode["i64_atomic_rmw_xchg"] = 65090] = "i64_atomic_rmw_xchg";
    OperatorCode[OperatorCode["i32_atomic_rmw8_xchg_u"] = 65091] = "i32_atomic_rmw8_xchg_u";
    OperatorCode[OperatorCode["i32_atomic_rmw16_xchg_u"] = 65092] = "i32_atomic_rmw16_xchg_u";
    OperatorCode[OperatorCode["i64_atomic_rmw8_xchg_u"] = 65093] = "i64_atomic_rmw8_xchg_u";
    OperatorCode[OperatorCode["i64_atomic_rmw16_xchg_u"] = 65094] = "i64_atomic_rmw16_xchg_u";
    OperatorCode[OperatorCode["i64_atomic_rmw32_xchg_u"] = 65095] = "i64_atomic_rmw32_xchg_u";
    OperatorCode[OperatorCode["i32_atomic_rmw_cmpxchg"] = 65096] = "i32_atomic_rmw_cmpxchg";
    OperatorCode[OperatorCode["i64_atomic_rmw_cmpxchg"] = 65097] = "i64_atomic_rmw_cmpxchg";
    OperatorCode[OperatorCode["i32_atomic_rmw8_cmpxchg_u"] = 65098] = "i32_atomic_rmw8_cmpxchg_u";
    OperatorCode[OperatorCode["i32_atomic_rmw16_cmpxchg_u"] = 65099] = "i32_atomic_rmw16_cmpxchg_u";
    OperatorCode[OperatorCode["i64_atomic_rmw8_cmpxchg_u"] = 65100] = "i64_atomic_rmw8_cmpxchg_u";
    OperatorCode[OperatorCode["i64_atomic_rmw16_cmpxchg_u"] = 65101] = "i64_atomic_rmw16_cmpxchg_u";
    OperatorCode[OperatorCode["i64_atomic_rmw32_cmpxchg_u"] = 65102] = "i64_atomic_rmw32_cmpxchg_u";
    OperatorCode[OperatorCode["v128_load"] = 64768] = "v128_load";
    OperatorCode[OperatorCode["i16x8_load8x8_s"] = 64769] = "i16x8_load8x8_s";
    OperatorCode[OperatorCode["i16x8_load8x8_u"] = 64770] = "i16x8_load8x8_u";
    OperatorCode[OperatorCode["i32x4_load16x4_s"] = 64771] = "i32x4_load16x4_s";
    OperatorCode[OperatorCode["i32x4_load16x4_u"] = 64772] = "i32x4_load16x4_u";
    OperatorCode[OperatorCode["i64x2_load32x2_s"] = 64773] = "i64x2_load32x2_s";
    OperatorCode[OperatorCode["i64x2_load32x2_u"] = 64774] = "i64x2_load32x2_u";
    OperatorCode[OperatorCode["v8x16_load_splat"] = 64775] = "v8x16_load_splat";
    OperatorCode[OperatorCode["v16x8_load_splat"] = 64776] = "v16x8_load_splat";
    OperatorCode[OperatorCode["v32x4_load_splat"] = 64777] = "v32x4_load_splat";
    OperatorCode[OperatorCode["v64x2_load_splat"] = 64778] = "v64x2_load_splat";
    OperatorCode[OperatorCode["v128_store"] = 64779] = "v128_store";
    OperatorCode[OperatorCode["v128_const"] = 64780] = "v128_const";
    OperatorCode[OperatorCode["i8x16_shuffle"] = 64781] = "i8x16_shuffle";
    OperatorCode[OperatorCode["i8x16_swizzle"] = 64782] = "i8x16_swizzle";
    OperatorCode[OperatorCode["i8x16_splat"] = 64783] = "i8x16_splat";
    OperatorCode[OperatorCode["i16x8_splat"] = 64784] = "i16x8_splat";
    OperatorCode[OperatorCode["i32x4_splat"] = 64785] = "i32x4_splat";
    OperatorCode[OperatorCode["i64x2_splat"] = 64786] = "i64x2_splat";
    OperatorCode[OperatorCode["f32x4_splat"] = 64787] = "f32x4_splat";
    OperatorCode[OperatorCode["f64x2_splat"] = 64788] = "f64x2_splat";
    OperatorCode[OperatorCode["i8x16_extract_lane_s"] = 64789] = "i8x16_extract_lane_s";
    OperatorCode[OperatorCode["i8x16_extract_lane_u"] = 64790] = "i8x16_extract_lane_u";
    OperatorCode[OperatorCode["i8x16_replace_lane"] = 64791] = "i8x16_replace_lane";
    OperatorCode[OperatorCode["i16x8_extract_lane_s"] = 64792] = "i16x8_extract_lane_s";
    OperatorCode[OperatorCode["i16x8_extract_lane_u"] = 64793] = "i16x8_extract_lane_u";
    OperatorCode[OperatorCode["i16x8_replace_lane"] = 64794] = "i16x8_replace_lane";
    OperatorCode[OperatorCode["i32x4_extract_lane"] = 64795] = "i32x4_extract_lane";
    OperatorCode[OperatorCode["i32x4_replace_lane"] = 64796] = "i32x4_replace_lane";
    OperatorCode[OperatorCode["i64x2_extract_lane"] = 64797] = "i64x2_extract_lane";
    OperatorCode[OperatorCode["i64x2_replace_lane"] = 64798] = "i64x2_replace_lane";
    OperatorCode[OperatorCode["f32x4_extract_lane"] = 64799] = "f32x4_extract_lane";
    OperatorCode[OperatorCode["f32x4_replace_lane"] = 64800] = "f32x4_replace_lane";
    OperatorCode[OperatorCode["f64x2_extract_lane"] = 64801] = "f64x2_extract_lane";
    OperatorCode[OperatorCode["f64x2_replace_lane"] = 64802] = "f64x2_replace_lane";
    OperatorCode[OperatorCode["i8x16_eq"] = 64803] = "i8x16_eq";
    OperatorCode[OperatorCode["i8x16_ne"] = 64804] = "i8x16_ne";
    OperatorCode[OperatorCode["i8x16_lt_s"] = 64805] = "i8x16_lt_s";
    OperatorCode[OperatorCode["i8x16_lt_u"] = 64806] = "i8x16_lt_u";
    OperatorCode[OperatorCode["i8x16_gt_s"] = 64807] = "i8x16_gt_s";
    OperatorCode[OperatorCode["i8x16_gt_u"] = 64808] = "i8x16_gt_u";
    OperatorCode[OperatorCode["i8x16_le_s"] = 64809] = "i8x16_le_s";
    OperatorCode[OperatorCode["i8x16_le_u"] = 64810] = "i8x16_le_u";
    OperatorCode[OperatorCode["i8x16_ge_s"] = 64811] = "i8x16_ge_s";
    OperatorCode[OperatorCode["i8x16_ge_u"] = 64812] = "i8x16_ge_u";
    OperatorCode[OperatorCode["i16x8_eq"] = 64813] = "i16x8_eq";
    OperatorCode[OperatorCode["i16x8_ne"] = 64814] = "i16x8_ne";
    OperatorCode[OperatorCode["i16x8_lt_s"] = 64815] = "i16x8_lt_s";
    OperatorCode[OperatorCode["i16x8_lt_u"] = 64816] = "i16x8_lt_u";
    OperatorCode[OperatorCode["i16x8_gt_s"] = 64817] = "i16x8_gt_s";
    OperatorCode[OperatorCode["i16x8_gt_u"] = 64818] = "i16x8_gt_u";
    OperatorCode[OperatorCode["i16x8_le_s"] = 64819] = "i16x8_le_s";
    OperatorCode[OperatorCode["i16x8_le_u"] = 64820] = "i16x8_le_u";
    OperatorCode[OperatorCode["i16x8_ge_s"] = 64821] = "i16x8_ge_s";
    OperatorCode[OperatorCode["i16x8_ge_u"] = 64822] = "i16x8_ge_u";
    OperatorCode[OperatorCode["i32x4_eq"] = 64823] = "i32x4_eq";
    OperatorCode[OperatorCode["i32x4_ne"] = 64824] = "i32x4_ne";
    OperatorCode[OperatorCode["i32x4_lt_s"] = 64825] = "i32x4_lt_s";
    OperatorCode[OperatorCode["i32x4_lt_u"] = 64826] = "i32x4_lt_u";
    OperatorCode[OperatorCode["i32x4_gt_s"] = 64827] = "i32x4_gt_s";
    OperatorCode[OperatorCode["i32x4_gt_u"] = 64828] = "i32x4_gt_u";
    OperatorCode[OperatorCode["i32x4_le_s"] = 64829] = "i32x4_le_s";
    OperatorCode[OperatorCode["i32x4_le_u"] = 64830] = "i32x4_le_u";
    OperatorCode[OperatorCode["i32x4_ge_s"] = 64831] = "i32x4_ge_s";
    OperatorCode[OperatorCode["i32x4_ge_u"] = 64832] = "i32x4_ge_u";
    OperatorCode[OperatorCode["f32x4_eq"] = 64833] = "f32x4_eq";
    OperatorCode[OperatorCode["f32x4_ne"] = 64834] = "f32x4_ne";
    OperatorCode[OperatorCode["f32x4_lt"] = 64835] = "f32x4_lt";
    OperatorCode[OperatorCode["f32x4_gt"] = 64836] = "f32x4_gt";
    OperatorCode[OperatorCode["f32x4_le"] = 64837] = "f32x4_le";
    OperatorCode[OperatorCode["f32x4_ge"] = 64838] = "f32x4_ge";
    OperatorCode[OperatorCode["f64x2_eq"] = 64839] = "f64x2_eq";
    OperatorCode[OperatorCode["f64x2_ne"] = 64840] = "f64x2_ne";
    OperatorCode[OperatorCode["f64x2_lt"] = 64841] = "f64x2_lt";
    OperatorCode[OperatorCode["f64x2_gt"] = 64842] = "f64x2_gt";
    OperatorCode[OperatorCode["f64x2_le"] = 64843] = "f64x2_le";
    OperatorCode[OperatorCode["f64x2_ge"] = 64844] = "f64x2_ge";
    OperatorCode[OperatorCode["v128_not"] = 64845] = "v128_not";
    OperatorCode[OperatorCode["v128_and"] = 64846] = "v128_and";
    OperatorCode[OperatorCode["v128_andnot"] = 64847] = "v128_andnot";
    OperatorCode[OperatorCode["v128_or"] = 64848] = "v128_or";
    OperatorCode[OperatorCode["v128_xor"] = 64849] = "v128_xor";
    OperatorCode[OperatorCode["v128_bitselect"] = 64850] = "v128_bitselect";
    OperatorCode[OperatorCode["v128_any_true"] = 64851] = "v128_any_true";
    OperatorCode[OperatorCode["v128_load8_lane"] = 64852] = "v128_load8_lane";
    OperatorCode[OperatorCode["v128_load16_lane"] = 64853] = "v128_load16_lane";
    OperatorCode[OperatorCode["v128_load32_lane"] = 64854] = "v128_load32_lane";
    OperatorCode[OperatorCode["v128_load64_lane"] = 64855] = "v128_load64_lane";
    OperatorCode[OperatorCode["v128_store8_lane"] = 64856] = "v128_store8_lane";
    OperatorCode[OperatorCode["v128_store16_lane"] = 64857] = "v128_store16_lane";
    OperatorCode[OperatorCode["v128_store32_lane"] = 64858] = "v128_store32_lane";
    OperatorCode[OperatorCode["v128_store64_lane"] = 64859] = "v128_store64_lane";
    OperatorCode[OperatorCode["v128_load32_zero"] = 64860] = "v128_load32_zero";
    OperatorCode[OperatorCode["v128_load64_zero"] = 64861] = "v128_load64_zero";
    OperatorCode[OperatorCode["f32x4_demote_f64x2_zero"] = 64862] = "f32x4_demote_f64x2_zero";
    OperatorCode[OperatorCode["f64x2_promote_low_f32x4"] = 64863] = "f64x2_promote_low_f32x4";
    OperatorCode[OperatorCode["i8x16_abs"] = 64864] = "i8x16_abs";
    OperatorCode[OperatorCode["i8x16_neg"] = 64865] = "i8x16_neg";
    OperatorCode[OperatorCode["i8x16_popcnt"] = 64866] = "i8x16_popcnt";
    OperatorCode[OperatorCode["i8x16_all_true"] = 64867] = "i8x16_all_true";
    OperatorCode[OperatorCode["i8x16_bitmask"] = 64868] = "i8x16_bitmask";
    OperatorCode[OperatorCode["i8x16_narrow_i16x8_s"] = 64869] = "i8x16_narrow_i16x8_s";
    OperatorCode[OperatorCode["i8x16_narrow_i16x8_u"] = 64870] = "i8x16_narrow_i16x8_u";
    OperatorCode[OperatorCode["f32x4_ceil"] = 103] = "f32x4_ceil";
    OperatorCode[OperatorCode["f32x4_floor"] = 104] = "f32x4_floor";
    OperatorCode[OperatorCode["f32x4_trunc"] = 105] = "f32x4_trunc";
    OperatorCode[OperatorCode["f32x4_nearest"] = 106] = "f32x4_nearest";
    OperatorCode[OperatorCode["i8x16_shl"] = 64875] = "i8x16_shl";
    OperatorCode[OperatorCode["i8x16_shr_s"] = 64876] = "i8x16_shr_s";
    OperatorCode[OperatorCode["i8x16_shr_u"] = 64877] = "i8x16_shr_u";
    OperatorCode[OperatorCode["i8x16_add"] = 64878] = "i8x16_add";
    OperatorCode[OperatorCode["i8x16_add_sat_s"] = 64879] = "i8x16_add_sat_s";
    OperatorCode[OperatorCode["i8x16_add_sat_u"] = 64880] = "i8x16_add_sat_u";
    OperatorCode[OperatorCode["i8x16_sub"] = 64881] = "i8x16_sub";
    OperatorCode[OperatorCode["i8x16_sub_sat_s"] = 64882] = "i8x16_sub_sat_s";
    OperatorCode[OperatorCode["i8x16_sub_sat_u"] = 64883] = "i8x16_sub_sat_u";
    OperatorCode[OperatorCode["f64x2_ceil"] = 64884] = "f64x2_ceil";
    OperatorCode[OperatorCode["f64x2_floor"] = 64885] = "f64x2_floor";
    OperatorCode[OperatorCode["i8x16_min_s"] = 64886] = "i8x16_min_s";
    OperatorCode[OperatorCode["i8x16_min_u"] = 64887] = "i8x16_min_u";
    OperatorCode[OperatorCode["i8x16_max_s"] = 64888] = "i8x16_max_s";
    OperatorCode[OperatorCode["i8x16_max_u"] = 64889] = "i8x16_max_u";
    OperatorCode[OperatorCode["f64x2_trunc"] = 64890] = "f64x2_trunc";
    OperatorCode[OperatorCode["i8x16_avgr_u"] = 64891] = "i8x16_avgr_u";
    OperatorCode[OperatorCode["i16x8_extadd_pairwise_i8x16_s"] = 64892] = "i16x8_extadd_pairwise_i8x16_s";
    OperatorCode[OperatorCode["i16x8_extadd_pairwise_i8x16_u"] = 64893] = "i16x8_extadd_pairwise_i8x16_u";
    OperatorCode[OperatorCode["i32x4_extadd_pairwise_i16x8_s"] = 64894] = "i32x4_extadd_pairwise_i16x8_s";
    OperatorCode[OperatorCode["i32x4_extadd_pairwise_i16x8_u"] = 64895] = "i32x4_extadd_pairwise_i16x8_u";
    OperatorCode[OperatorCode["i16x8_abs"] = 64896] = "i16x8_abs";
    OperatorCode[OperatorCode["i16x8_neg"] = 64897] = "i16x8_neg";
    OperatorCode[OperatorCode["i16x8_q15mulr_sat_s"] = 64898] = "i16x8_q15mulr_sat_s";
    OperatorCode[OperatorCode["i16x8_all_true"] = 64899] = "i16x8_all_true";
    OperatorCode[OperatorCode["i16x8_bitmask"] = 64900] = "i16x8_bitmask";
    OperatorCode[OperatorCode["i16x8_narrow_i32x4_s"] = 64901] = "i16x8_narrow_i32x4_s";
    OperatorCode[OperatorCode["i16x8_narrow_i32x4_u"] = 64902] = "i16x8_narrow_i32x4_u";
    OperatorCode[OperatorCode["i16x8_extend_low_i8x16_s"] = 64903] = "i16x8_extend_low_i8x16_s";
    OperatorCode[OperatorCode["i16x8_extend_high_i8x16_s"] = 64904] = "i16x8_extend_high_i8x16_s";
    OperatorCode[OperatorCode["i16x8_extend_low_i8x16_u"] = 64905] = "i16x8_extend_low_i8x16_u";
    OperatorCode[OperatorCode["i16x8_extend_high_i8x16_u"] = 64906] = "i16x8_extend_high_i8x16_u";
    OperatorCode[OperatorCode["i16x8_shl"] = 64907] = "i16x8_shl";
    OperatorCode[OperatorCode["i16x8_shr_s"] = 64908] = "i16x8_shr_s";
    OperatorCode[OperatorCode["i16x8_shr_u"] = 64909] = "i16x8_shr_u";
    OperatorCode[OperatorCode["i16x8_add"] = 64910] = "i16x8_add";
    OperatorCode[OperatorCode["i16x8_add_sat_s"] = 64911] = "i16x8_add_sat_s";
    OperatorCode[OperatorCode["i16x8_add_sat_u"] = 64912] = "i16x8_add_sat_u";
    OperatorCode[OperatorCode["i16x8_sub"] = 64913] = "i16x8_sub";
    OperatorCode[OperatorCode["i16x8_sub_sat_s"] = 64914] = "i16x8_sub_sat_s";
    OperatorCode[OperatorCode["i16x8_sub_sat_u"] = 64915] = "i16x8_sub_sat_u";
    OperatorCode[OperatorCode["f64x2_nearest"] = 64916] = "f64x2_nearest";
    OperatorCode[OperatorCode["i16x8_mul"] = 64917] = "i16x8_mul";
    OperatorCode[OperatorCode["i16x8_min_s"] = 64918] = "i16x8_min_s";
    OperatorCode[OperatorCode["i16x8_min_u"] = 64919] = "i16x8_min_u";
    OperatorCode[OperatorCode["i16x8_max_s"] = 64920] = "i16x8_max_s";
    OperatorCode[OperatorCode["i16x8_max_u"] = 64921] = "i16x8_max_u";
    OperatorCode[OperatorCode["i16x8_avgr_u"] = 64923] = "i16x8_avgr_u";
    OperatorCode[OperatorCode["i16x8_extmul_low_i8x16_s"] = 64924] = "i16x8_extmul_low_i8x16_s";
    OperatorCode[OperatorCode["i16x8_extmul_high_i8x16_s"] = 64925] = "i16x8_extmul_high_i8x16_s";
    OperatorCode[OperatorCode["i16x8_extmul_low_i8x16_u"] = 64926] = "i16x8_extmul_low_i8x16_u";
    OperatorCode[OperatorCode["i16x8_extmul_high_i8x16_u"] = 64927] = "i16x8_extmul_high_i8x16_u";
    OperatorCode[OperatorCode["i32x4_abs"] = 64928] = "i32x4_abs";
    OperatorCode[OperatorCode["i32x4_neg"] = 64929] = "i32x4_neg";
    OperatorCode[OperatorCode["i32x4_all_true"] = 64931] = "i32x4_all_true";
    OperatorCode[OperatorCode["i32x4_bitmask"] = 64932] = "i32x4_bitmask";
    OperatorCode[OperatorCode["i32x4_extend_low_i16x8_s"] = 64935] = "i32x4_extend_low_i16x8_s";
    OperatorCode[OperatorCode["i32x4_extend_high_i16x8_s"] = 64936] = "i32x4_extend_high_i16x8_s";
    OperatorCode[OperatorCode["i32x4_extend_low_i16x8_u"] = 64937] = "i32x4_extend_low_i16x8_u";
    OperatorCode[OperatorCode["i32x4_extend_high_i16x8_u"] = 64938] = "i32x4_extend_high_i16x8_u";
    OperatorCode[OperatorCode["i32x4_shl"] = 64939] = "i32x4_shl";
    OperatorCode[OperatorCode["i32x4_shr_s"] = 64940] = "i32x4_shr_s";
    OperatorCode[OperatorCode["i32x4_shr_u"] = 64941] = "i32x4_shr_u";
    OperatorCode[OperatorCode["i32x4_add"] = 64942] = "i32x4_add";
    OperatorCode[OperatorCode["i32x4_sub"] = 64945] = "i32x4_sub";
    OperatorCode[OperatorCode["i32x4_mul"] = 64949] = "i32x4_mul";
    OperatorCode[OperatorCode["i32x4_min_s"] = 64950] = "i32x4_min_s";
    OperatorCode[OperatorCode["i32x4_min_u"] = 64951] = "i32x4_min_u";
    OperatorCode[OperatorCode["i32x4_max_s"] = 64952] = "i32x4_max_s";
    OperatorCode[OperatorCode["i32x4_max_u"] = 64953] = "i32x4_max_u";
    OperatorCode[OperatorCode["i32x4_dot_i16x8_s"] = 64954] = "i32x4_dot_i16x8_s";
    OperatorCode[OperatorCode["i32x4_extmul_low_i16x8_s"] = 64956] = "i32x4_extmul_low_i16x8_s";
    OperatorCode[OperatorCode["i32x4_extmul_high_i16x8_s"] = 64957] = "i32x4_extmul_high_i16x8_s";
    OperatorCode[OperatorCode["i32x4_extmul_low_i16x8_u"] = 64958] = "i32x4_extmul_low_i16x8_u";
    OperatorCode[OperatorCode["i32x4_extmul_high_i16x8_u"] = 64959] = "i32x4_extmul_high_i16x8_u";
    OperatorCode[OperatorCode["i64x2_abs"] = 64960] = "i64x2_abs";
    OperatorCode[OperatorCode["i64x2_neg"] = 64961] = "i64x2_neg";
    OperatorCode[OperatorCode["i64x2_all_true"] = 64963] = "i64x2_all_true";
    OperatorCode[OperatorCode["i64x2_bitmask"] = 64964] = "i64x2_bitmask";
    OperatorCode[OperatorCode["i64x2_extend_low_i32x4_s"] = 64967] = "i64x2_extend_low_i32x4_s";
    OperatorCode[OperatorCode["i64x2_extend_high_i32x4_s"] = 64968] = "i64x2_extend_high_i32x4_s";
    OperatorCode[OperatorCode["i64x2_extend_low_i32x4_u"] = 64969] = "i64x2_extend_low_i32x4_u";
    OperatorCode[OperatorCode["i64x2_extend_high_i32x4_u"] = 64970] = "i64x2_extend_high_i32x4_u";
    OperatorCode[OperatorCode["i64x2_shl"] = 64971] = "i64x2_shl";
    OperatorCode[OperatorCode["i64x2_shr_s"] = 64972] = "i64x2_shr_s";
    OperatorCode[OperatorCode["i64x2_shr_u"] = 64973] = "i64x2_shr_u";
    OperatorCode[OperatorCode["i64x2_add"] = 64974] = "i64x2_add";
    OperatorCode[OperatorCode["i64x2_sub"] = 64977] = "i64x2_sub";
    OperatorCode[OperatorCode["i64x2_mul"] = 64981] = "i64x2_mul";
    OperatorCode[OperatorCode["i64x2_eq"] = 64982] = "i64x2_eq";
    OperatorCode[OperatorCode["i64x2_ne"] = 64983] = "i64x2_ne";
    OperatorCode[OperatorCode["i64x2_lt_s"] = 64984] = "i64x2_lt_s";
    OperatorCode[OperatorCode["i64x2_gt_s"] = 64985] = "i64x2_gt_s";
    OperatorCode[OperatorCode["i64x2_le_s"] = 64986] = "i64x2_le_s";
    OperatorCode[OperatorCode["i64x2_ge_s"] = 64987] = "i64x2_ge_s";
    OperatorCode[OperatorCode["i64x2_extmul_low_i32x4_s"] = 64988] = "i64x2_extmul_low_i32x4_s";
    OperatorCode[OperatorCode["i64x2_extmul_high_i32x4_s"] = 64989] = "i64x2_extmul_high_i32x4_s";
    OperatorCode[OperatorCode["i64x2_extmul_low_i32x4_u"] = 64990] = "i64x2_extmul_low_i32x4_u";
    OperatorCode[OperatorCode["i64x2_extmul_high_i32x4_u"] = 64991] = "i64x2_extmul_high_i32x4_u";
    OperatorCode[OperatorCode["f32x4_abs"] = 64992] = "f32x4_abs";
    OperatorCode[OperatorCode["f32x4_neg"] = 64993] = "f32x4_neg";
    OperatorCode[OperatorCode["f32x4_sqrt"] = 64995] = "f32x4_sqrt";
    OperatorCode[OperatorCode["f32x4_add"] = 64996] = "f32x4_add";
    OperatorCode[OperatorCode["f32x4_sub"] = 64997] = "f32x4_sub";
    OperatorCode[OperatorCode["f32x4_mul"] = 64998] = "f32x4_mul";
    OperatorCode[OperatorCode["f32x4_div"] = 64999] = "f32x4_div";
    OperatorCode[OperatorCode["f32x4_min"] = 65000] = "f32x4_min";
    OperatorCode[OperatorCode["f32x4_max"] = 65001] = "f32x4_max";
    OperatorCode[OperatorCode["f32x4_pmin"] = 65002] = "f32x4_pmin";
    OperatorCode[OperatorCode["f32x4_pmax"] = 65003] = "f32x4_pmax";
    OperatorCode[OperatorCode["f64x2_abs"] = 65004] = "f64x2_abs";
    OperatorCode[OperatorCode["f64x2_neg"] = 65005] = "f64x2_neg";
    OperatorCode[OperatorCode["f64x2_sqrt"] = 65007] = "f64x2_sqrt";
    OperatorCode[OperatorCode["f64x2_add"] = 65008] = "f64x2_add";
    OperatorCode[OperatorCode["f64x2_sub"] = 65009] = "f64x2_sub";
    OperatorCode[OperatorCode["f64x2_mul"] = 65010] = "f64x2_mul";
    OperatorCode[OperatorCode["f64x2_div"] = 65011] = "f64x2_div";
    OperatorCode[OperatorCode["f64x2_min"] = 65012] = "f64x2_min";
    OperatorCode[OperatorCode["f64x2_max"] = 65013] = "f64x2_max";
    OperatorCode[OperatorCode["f64x2_pmin"] = 65014] = "f64x2_pmin";
    OperatorCode[OperatorCode["f64x2_pmax"] = 65015] = "f64x2_pmax";
    OperatorCode[OperatorCode["i32x4_trunc_sat_f32x4_s"] = 65016] = "i32x4_trunc_sat_f32x4_s";
    OperatorCode[OperatorCode["i32x4_trunc_sat_f32x4_u"] = 65017] = "i32x4_trunc_sat_f32x4_u";
    OperatorCode[OperatorCode["f32x4_convert_i32x4_s"] = 65018] = "f32x4_convert_i32x4_s";
    OperatorCode[OperatorCode["f32x4_convert_i32x4_u"] = 65019] = "f32x4_convert_i32x4_u";
    OperatorCode[OperatorCode["i32x4_trunc_sat_f64x2_s_zero"] = 65020] = "i32x4_trunc_sat_f64x2_s_zero";
    OperatorCode[OperatorCode["i32x4_trunc_sat_f64x2_u_zero"] = 65021] = "i32x4_trunc_sat_f64x2_u_zero";
    OperatorCode[OperatorCode["f64x2_convert_low_i32x4_s"] = 65022] = "f64x2_convert_low_i32x4_s";
    OperatorCode[OperatorCode["f64x2_convert_low_i32x4_u"] = 65023] = "f64x2_convert_low_i32x4_u";
    // GC proposal.
    OperatorCode[OperatorCode["struct_new_with_rtt"] = 64257] = "struct_new_with_rtt";
    OperatorCode[OperatorCode["struct_new_default_with_rtt"] = 64258] = "struct_new_default_with_rtt";
    OperatorCode[OperatorCode["struct_get"] = 64259] = "struct_get";
    OperatorCode[OperatorCode["struct_get_s"] = 64260] = "struct_get_s";
    OperatorCode[OperatorCode["struct_get_u"] = 64261] = "struct_get_u";
    OperatorCode[OperatorCode["struct_set"] = 64262] = "struct_set";
    OperatorCode[OperatorCode["array_new_with_rtt"] = 64273] = "array_new_with_rtt";
    OperatorCode[OperatorCode["array_new_default_with_rtt"] = 64274] = "array_new_default_with_rtt";
    OperatorCode[OperatorCode["array_get"] = 64275] = "array_get";
    OperatorCode[OperatorCode["array_get_s"] = 64276] = "array_get_s";
    OperatorCode[OperatorCode["array_get_u"] = 64277] = "array_get_u";
    OperatorCode[OperatorCode["array_set"] = 64278] = "array_set";
    OperatorCode[OperatorCode["array_len"] = 64279] = "array_len";
    OperatorCode[OperatorCode["i31_new"] = 64288] = "i31_new";
    OperatorCode[OperatorCode["i31_get_s"] = 64289] = "i31_get_s";
    OperatorCode[OperatorCode["i31_get_u"] = 64290] = "i31_get_u";
    OperatorCode[OperatorCode["rtt_canon"] = 64304] = "rtt_canon";
    OperatorCode[OperatorCode["rtt_sub"] = 64305] = "rtt_sub";
    OperatorCode[OperatorCode["ref_test"] = 64320] = "ref_test";
    OperatorCode[OperatorCode["ref_cast"] = 64321] = "ref_cast";
    OperatorCode[OperatorCode["br_on_cast"] = 64322] = "br_on_cast";
    OperatorCode[OperatorCode["ref_is_func"] = 64336] = "ref_is_func";
    OperatorCode[OperatorCode["ref_is_data"] = 64337] = "ref_is_data";
    OperatorCode[OperatorCode["ref_is_i31"] = 64338] = "ref_is_i31";
    OperatorCode[OperatorCode["ref_as_func"] = 64344] = "ref_as_func";
    OperatorCode[OperatorCode["ref_as_data"] = 64345] = "ref_as_data";
    OperatorCode[OperatorCode["ref_as_i31"] = 64346] = "ref_as_i31";
    OperatorCode[OperatorCode["br_on_func"] = 64352] = "br_on_func";
    OperatorCode[OperatorCode["br_on_data"] = 64353] = "br_on_data";
    OperatorCode[OperatorCode["br_on_i31"] = 64354] = "br_on_i31";
})(OperatorCode = exports.OperatorCode || (exports.OperatorCode = {}));
exports.OperatorCodeNames = [
    "unreachable",
    "nop",
    "block",
    "loop",
    "if",
    "else",
    "try",
    "catch",
    "throw",
    "rethrow",
    "unwind",
    "end",
    "br",
    "br_if",
    "br_table",
    "return",
    "call",
    "call_indirect",
    "return_call",
    "return_call_indirect",
    "call_ref",
    "return_call_ref",
    undefined,
    "let",
    "delegate",
    "catch_all",
    "drop",
    "select",
    undefined,
    undefined,
    undefined,
    undefined,
    "local.get",
    "local.set",
    "local.tee",
    "global.get",
    "global.set",
    "table.get",
    "table.set",
    undefined,
    "i32.load",
    "i64.load",
    "f32.load",
    "f64.load",
    "i32.load8_s",
    "i32.load8_u",
    "i32.load16_s",
    "i32.load16_u",
    "i64.load8_s",
    "i64.load8_u",
    "i64.load16_s",
    "i64.load16_u",
    "i64.load32_s",
    "i64.load32_u",
    "i32.store",
    "i64.store",
    "f32.store",
    "f64.store",
    "i32.store8",
    "i32.store16",
    "i64.store8",
    "i64.store16",
    "i64.store32",
    "current_memory",
    "memory.grow",
    "i32.const",
    "i64.const",
    "f32.const",
    "f64.const",
    "i32.eqz",
    "i32.eq",
    "i32.ne",
    "i32.lt_s",
    "i32.lt_u",
    "i32.gt_s",
    "i32.gt_u",
    "i32.le_s",
    "i32.le_u",
    "i32.ge_s",
    "i32.ge_u",
    "i64.eqz",
    "i64.eq",
    "i64.ne",
    "i64.lt_s",
    "i64.lt_u",
    "i64.gt_s",
    "i64.gt_u",
    "i64.le_s",
    "i64.le_u",
    "i64.ge_s",
    "i64.ge_u",
    "f32.eq",
    "f32.ne",
    "f32.lt",
    "f32.gt",
    "f32.le",
    "f32.ge",
    "f64.eq",
    "f64.ne",
    "f64.lt",
    "f64.gt",
    "f64.le",
    "f64.ge",
    "i32.clz",
    "i32.ctz",
    "i32.popcnt",
    "i32.add",
    "i32.sub",
    "i32.mul",
    "i32.div_s",
    "i32.div_u",
    "i32.rem_s",
    "i32.rem_u",
    "i32.and",
    "i32.or",
    "i32.xor",
    "i32.shl",
    "i32.shr_s",
    "i32.shr_u",
    "i32.rotl",
    "i32.rotr",
    "i64.clz",
    "i64.ctz",
    "i64.popcnt",
    "i64.add",
    "i64.sub",
    "i64.mul",
    "i64.div_s",
    "i64.div_u",
    "i64.rem_s",
    "i64.rem_u",
    "i64.and",
    "i64.or",
    "i64.xor",
    "i64.shl",
    "i64.shr_s",
    "i64.shr_u",
    "i64.rotl",
    "i64.rotr",
    "f32.abs",
    "f32.neg",
    "f32.ceil",
    "f32.floor",
    "f32.trunc",
    "f32.nearest",
    "f32.sqrt",
    "f32.add",
    "f32.sub",
    "f32.mul",
    "f32.div",
    "f32.min",
    "f32.max",
    "f32.copysign",
    "f64.abs",
    "f64.neg",
    "f64.ceil",
    "f64.floor",
    "f64.trunc",
    "f64.nearest",
    "f64.sqrt",
    "f64.add",
    "f64.sub",
    "f64.mul",
    "f64.div",
    "f64.min",
    "f64.max",
    "f64.copysign",
    "i32.wrap_i64",
    "i32.trunc_f32_s",
    "i32.trunc_f32_u",
    "i32.trunc_f64_s",
    "i32.trunc_f64_u",
    "i64.extend_i32_s",
    "i64.extend_i32_u",
    "i64.trunc_f32_s",
    "i64.trunc_f32_u",
    "i64.trunc_f64_s",
    "i64.trunc_f64_u",
    "f32.convert_i32_s",
    "f32.convert_i32_u",
    "f32.convert_i64_s",
    "f32.convert_i64_u",
    "f32.demote_f64",
    "f64.convert_i32_s",
    "f64.convert_i32_u",
    "f64.convert_i64_s",
    "f64.convert_i64_u",
    "f64.promote_f32",
    "i32.reinterpret_f32",
    "i64.reinterpret_f64",
    "f32.reinterpret_i32",
    "f64.reinterpret_i64",
    "i32.extend8_s",
    "i32.extend16_s",
    "i64.extend8_s",
    "i64.extend16_s",
    "i64.extend32_s",
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    "ref.null",
    "ref.is_null",
    "ref.func",
    "ref.as_non_null",
    "br_on_null",
    "ref.eq",
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
];
[
    "i32.trunc_sat_f32_s",
    "i32.trunc_sat_f32_u",
    "i32.trunc_sat_f64_s",
    "i32.trunc_sat_f64_u",
    "i64.trunc_sat_f32_s",
    "i64.trunc_sat_f32_u",
    "i64.trunc_sat_f64_s",
    "i64.trunc_sat_f64_u",
    "memory.init",
    "data.drop",
    "memory.copy",
    "memory.fill",
    "table.init",
    "elem.drop",
    "table.copy",
    "table.grow",
    "table.size",
    "table.fill",
].forEach(function (s, i) {
    exports.OperatorCodeNames[0xfc00 | i] = s;
});
[
    "v128.load",
    "i16x8.load8x8_s",
    "i16x8.load8x8_u",
    "i32x4.load16x4_s",
    "i32x4.load16x4_u",
    "i64x2.load32x2_s",
    "i64x2.load32x2_u",
    "v8x16.load_splat",
    "v16x8.load_splat",
    "v32x4.load_splat",
    "v64x2.load_splat",
    "v128.store",
    "v128.const",
    "i8x16.shuffle",
    "i8x16.swizzle",
    "i8x16.splat",
    "i16x8.splat",
    "i32x4.splat",
    "i64x2.splat",
    "f32x4.splat",
    "f64x2.splat",
    "i8x16.extract_lane_s",
    "i8x16.extract_lane_u",
    "i8x16.replace_lane",
    "i16x8.extract_lane_s",
    "i16x8.extract_lane_u",
    "i16x8.replace_lane",
    "i32x4.extract_lane",
    "i32x4.replace_lane",
    "i64x2.extract_lane",
    "i64x2.replace_lane",
    "f32x4.extract_lane",
    "f32x4.replace_lane",
    "f64x2.extract_lane",
    "f64x2.replace_lane",
    "i8x16.eq",
    "i8x16.ne",
    "i8x16.lt_s",
    "i8x16.lt_u",
    "i8x16.gt_s",
    "i8x16.gt_u",
    "i8x16.le_s",
    "i8x16.le_u",
    "i8x16.ge_s",
    "i8x16.ge_u",
    "i16x8.eq",
    "i16x8.ne",
    "i16x8.lt_s",
    "i16x8.lt_u",
    "i16x8.gt_s",
    "i16x8.gt_u",
    "i16x8.le_s",
    "i16x8.le_u",
    "i16x8.ge_s",
    "i16x8.ge_u",
    "i32x4.eq",
    "i32x4.ne",
    "i32x4.lt_s",
    "i32x4.lt_u",
    "i32x4.gt_s",
    "i32x4.gt_u",
    "i32x4.le_s",
    "i32x4.le_u",
    "i32x4.ge_s",
    "i32x4.ge_u",
    "f32x4.eq",
    "f32x4.ne",
    "f32x4.lt",
    "f32x4.gt",
    "f32x4.le",
    "f32x4.ge",
    "f64x2.eq",
    "f64x2.ne",
    "f64x2.lt",
    "f64x2.gt",
    "f64x2.le",
    "f64x2.ge",
    "v128.not",
    "v128.and",
    "v128.andnot",
    "v128.or",
    "v128.xor",
    "v128.bitselect",
    "v128.any_true",
    "v128.load8_lane",
    "v128.load16_lane",
    "v128.load32_lane",
    "v128.load64_lane",
    "v128.store8_lane",
    "v128.store16_lane",
    "v128.store32_lane",
    "v128.store64_lane",
    "v128.load32_zero",
    "v128.load64_zero",
    "f32x4.demote_f64x2_zero",
    "f64x2.promote_low_f32x4",
    "i8x16.abs",
    "i8x16.neg",
    "i8x16_popcnt",
    "i8x16.all_true",
    "i8x16.bitmask",
    "i8x16.narrow_i16x8_s",
    "i8x16.narrow_i16x8_u",
    "f32x4.ceil",
    "f32x4.floor",
    "f32x4.trunc",
    "f32x4.nearest",
    "i8x16.shl",
    "i8x16.shr_s",
    "i8x16.shr_u",
    "i8x16.add",
    "i8x16.add_sat_s",
    "i8x16.add_sat_u",
    "i8x16.sub",
    "i8x16.sub_sat_s",
    "i8x16.sub_sat_u",
    "f64x2.ceil",
    "f64x2.floor",
    "i8x16.min_s",
    "i8x16.min_u",
    "i8x16.max_s",
    "i8x16.max_u",
    "f64x2.trunc",
    "i8x16.avgr_u",
    "i16x8.extadd_pairwise_i8x16_s",
    "i16x8.extadd_pairwise_i8x16_u",
    "i32x4.extadd_pairwise_i16x8_s",
    "i32x4.extadd_pairwise_i16x8_u",
    "i16x8.abs",
    "i16x8.neg",
    "i16x8.q15mulr_sat_s",
    "i16x8.all_true",
    "i16x8.bitmask",
    "i16x8.narrow_i32x4_s",
    "i16x8.narrow_i32x4_u",
    "i16x8.extend_low_i8x16_s",
    "i16x8.extend_high_i8x16_s",
    "i16x8.extend_low_i8x16_u",
    "i16x8.extend_high_i8x16_u",
    "i16x8.shl",
    "i16x8.shr_s",
    "i16x8.shr_u",
    "i16x8.add",
    "i16x8.add_sat_s",
    "i16x8.add_sat_u",
    "i16x8.sub",
    "i16x8.sub_sat_s",
    "i16x8.sub_sat_u",
    "f64x2.nearest",
    "i16x8.mul",
    "i16x8.min_s",
    "i16x8.min_u",
    "i16x8.max_s",
    "i16x8.max_u",
    undefined,
    "i16x8.avgr_u",
    "i16x8.extmul_low_i8x16_s",
    "i16x8.extmul_high_i8x16_s",
    "i16x8.extmul_low_i8x16_u",
    "i16x8.extmul_high_i8x16_u",
    "i32x4.abs",
    "i32x4.neg",
    undefined,
    "i32x4.all_true",
    "i32x4.bitmask",
    undefined,
    undefined,
    "i32x4.extend_low_i16x8_s",
    "i32x4.extend_high_i16x8_s",
    "i32x4.extend_low_i16x8_u",
    "i32x4.extend_high_i16x8_u",
    "i32x4.shl",
    "i32x4.shr_s",
    "i32x4.shr_u",
    "i32x4.add",
    undefined,
    undefined,
    "i32x4.sub",
    undefined,
    undefined,
    undefined,
    "i32x4.mul",
    "i32x4.min_s",
    "i32x4.min_u",
    "i32x4.max_s",
    "i32x4.max_u",
    "i32x4.dot_i16x8_s",
    undefined,
    "i32x4.extmul_low_i16x8_s",
    "i32x4.extmul_high_i16x8_s",
    "i32x4.extmul_low_i16x8_u",
    "i32x4.extmul_high_i16x8_u",
    "i64x2.abs",
    "i64x2.neg",
    undefined,
    "i64x2.all_true",
    "i64x2.bitmask",
    undefined,
    undefined,
    "i64x2.extend_low_i32x4_s",
    "i64x2.extend_high_i32x4_s",
    "i64x2.extend_low_i32x4_u",
    "i64x2.extend_high_i32x4_u",
    "i64x2.shl",
    "i64x2.shr_s",
    "i64x2.shr_u",
    "i64x2.add",
    undefined,
    undefined,
    "i64x2.sub",
    undefined,
    undefined,
    undefined,
    "i64x2.mul",
    "i64x2.eq",
    "i64x2.ne",
    "i64x2.lt_s",
    "i64x2.gt_s",
    "i64x2.le_s",
    "i64x2.ge_s",
    "i64x2.extmul_low_i32x4_s",
    "i64x2.extmul_high_i32x4_s",
    "i64x2.extmul_low_i32x4_u",
    "i64x2.extmul_high_i32x4_u",
    "f32x4.abs",
    "f32x4.neg",
    undefined,
    "f32x4.sqrt",
    "f32x4.add",
    "f32x4.sub",
    "f32x4.mul",
    "f32x4.div",
    "f32x4.min",
    "f32x4.max",
    "f32x4.pmin",
    "f32x4.pmax",
    "f64x2.abs",
    "f64x2.neg",
    undefined,
    "f64x2.sqrt",
    "f64x2.add",
    "f64x2.sub",
    "f64x2.mul",
    "f64x2.div",
    "f64x2.min",
    "f64x2.max",
    "f64x2.pmin",
    "f64x2.pmax",
    "i32x4.trunc_sat_f32x4_s",
    "i32x4.trunc_sat_f32x4_u",
    "f32x4.convert_i32x4_s",
    "f32x4.convert_i32x4_u",
    "i32x4.trunc_sat_f64x2_s_zero",
    "i32x4.trunc_sat_f64x2_u_zero",
    "f64x2.convert_low_i32x4_s",
    "f64x2.convert_low_i32x4_u",
].forEach(function (s, i) {
    exports.OperatorCodeNames[0xfd00 | i] = s;
});
[
    "atomic.notify",
    "i32.atomic.wait",
    "i64.atomic.wait",
    "atomic.fence",
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    undefined,
    "i32.atomic.load",
    "i64.atomic.load",
    "i32.atomic.load8_u",
    "i32.atomic.load16_u",
    "i64.atomic.load8_u",
    "i64.atomic.load16_u",
    "i64.atomic.load32_u",
    "i32.atomic.store",
    "i64.atomic.store",
    "i32.atomic.store8",
    "i32.atomic.store16",
    "i64.atomic.store8",
    "i64.atomic.store16",
    "i64.atomic.store32",
    "i32.atomic.rmw.add",
    "i64.atomic.rmw.add",
    "i32.atomic.rmw8.add_u",
    "i32.atomic.rmw16.add_u",
    "i64.atomic.rmw8.add_u",
    "i64.atomic.rmw16.add_u",
    "i64.atomic.rmw32.add_u",
    "i32.atomic.rmw.sub",
    "i64.atomic.rmw.sub",
    "i32.atomic.rmw8.sub_u",
    "i32.atomic.rmw16.sub_u",
    "i64.atomic.rmw8.sub_u",
    "i64.atomic.rmw16.sub_u",
    "i64.atomic.rmw32.sub_u",
    "i32.atomic.rmw.and",
    "i64.atomic.rmw.and",
    "i32.atomic.rmw8.and_u",
    "i32.atomic.rmw16.and_u",
    "i64.atomic.rmw8.and_u",
    "i64.atomic.rmw16.and_u",
    "i64.atomic.rmw32.and_u",
    "i32.atomic.rmw.or",
    "i64.atomic.rmw.or",
    "i32.atomic.rmw8.or_u",
    "i32.atomic.rmw16.or_u",
    "i64.atomic.rmw8.or_u",
    "i64.atomic.rmw16.or_u",
    "i64.atomic.rmw32.or_u",
    "i32.atomic.rmw.xor",
    "i64.atomic.rmw.xor",
    "i32.atomic.rmw8.xor_u",
    "i32.atomic.rmw16.xor_u",
    "i64.atomic.rmw8.xor_u",
    "i64.atomic.rmw16.xor_u",
    "i64.atomic.rmw32.xor_u",
    "i32.atomic.rmw.xchg",
    "i64.atomic.rmw.xchg",
    "i32.atomic.rmw8.xchg_u",
    "i32.atomic.rmw16.xchg_u",
    "i64.atomic.rmw8.xchg_u",
    "i64.atomic.rmw16.xchg_u",
    "i64.atomic.rmw32.xchg_u",
    "i32.atomic.rmw.cmpxchg",
    "i64.atomic.rmw.cmpxchg",
    "i32.atomic.rmw8.cmpxchg_u",
    "i32.atomic.rmw16.cmpxchg_u",
    "i64.atomic.rmw8.cmpxchg_u",
    "i64.atomic.rmw16.cmpxchg_u",
    "i64.atomic.rmw32.cmpxchg_u",
].forEach(function (s, i) {
    exports.OperatorCodeNames[0xfe00 | i] = s;
});
exports.OperatorCodeNames[0xfb01] = "struct.new_with_rtt";
exports.OperatorCodeNames[0xfb02] = "struct.new_default_with_rtt";
exports.OperatorCodeNames[0xfb03] = "struct.get";
exports.OperatorCodeNames[0xfb04] = "struct.get_s";
exports.OperatorCodeNames[0xfb05] = "struct.get_u";
exports.OperatorCodeNames[0xfb06] = "struct.set";
exports.OperatorCodeNames[0xfb11] = "array.new_with_rtt";
exports.OperatorCodeNames[0xfb12] = "array.new_default_with_rtt";
exports.OperatorCodeNames[0xfb13] = "array.get";
exports.OperatorCodeNames[0xfb14] = "array.get_s";
exports.OperatorCodeNames[0xfb15] = "array.get_u";
exports.OperatorCodeNames[0xfb16] = "array.set";
exports.OperatorCodeNames[0xfb17] = "array.len";
exports.OperatorCodeNames[0xfb20] = "i31.new";
exports.OperatorCodeNames[0xfb21] = "i31.get_s";
exports.OperatorCodeNames[0xfb22] = "i31.get_u";
exports.OperatorCodeNames[0xfb30] = "rtt.canon";
exports.OperatorCodeNames[0xfb31] = "rtt.sub";
exports.OperatorCodeNames[0xfb40] = "ref.test";
exports.OperatorCodeNames[0xfb41] = "ref.cast";
exports.OperatorCodeNames[0xfb42] = "br_on_cast";
exports.OperatorCodeNames[0xfb50] = "ref.is_func";
exports.OperatorCodeNames[0xfb51] = "ref.is_data";
exports.OperatorCodeNames[0xfb52] = "ref.is_i31";
exports.OperatorCodeNames[0xfb58] = "ref.as_func";
exports.OperatorCodeNames[0xfb59] = "ref.as_data";
exports.OperatorCodeNames[0xfb5a] = "ref.as_i31";
exports.OperatorCodeNames[0xfb60] = "br_on_func";
exports.OperatorCodeNames[0xfb61] = "br_on_data";
exports.OperatorCodeNames[0xfb62] = "br_on_i31";
var ExternalKind;
(function (ExternalKind) {
    ExternalKind[ExternalKind["Function"] = 0] = "Function";
    ExternalKind[ExternalKind["Table"] = 1] = "Table";
    ExternalKind[ExternalKind["Memory"] = 2] = "Memory";
    ExternalKind[ExternalKind["Global"] = 3] = "Global";
    ExternalKind[ExternalKind["Event"] = 4] = "Event";
})(ExternalKind = exports.ExternalKind || (exports.ExternalKind = {}));
var TypeKind;
(function (TypeKind) {
    TypeKind[TypeKind["unspecified"] = 0] = "unspecified";
    TypeKind[TypeKind["i32"] = -1] = "i32";
    TypeKind[TypeKind["i64"] = -2] = "i64";
    TypeKind[TypeKind["f32"] = -3] = "f32";
    TypeKind[TypeKind["f64"] = -4] = "f64";
    TypeKind[TypeKind["v128"] = -5] = "v128";
    TypeKind[TypeKind["i8"] = -6] = "i8";
    TypeKind[TypeKind["i16"] = -7] = "i16";
    TypeKind[TypeKind["funcref"] = -16] = "funcref";
    TypeKind[TypeKind["externref"] = -17] = "externref";
    TypeKind[TypeKind["anyref"] = -18] = "anyref";
    TypeKind[TypeKind["eqref"] = -19] = "eqref";
    TypeKind[TypeKind["optref"] = -20] = "optref";
    TypeKind[TypeKind["ref"] = -21] = "ref";
    TypeKind[TypeKind["i31ref"] = -22] = "i31ref";
    TypeKind[TypeKind["rtt_d"] = -23] = "rtt_d";
    TypeKind[TypeKind["rtt"] = -24] = "rtt";
    TypeKind[TypeKind["dataref"] = -25] = "dataref";
    TypeKind[TypeKind["func"] = -32] = "func";
    TypeKind[TypeKind["struct"] = -33] = "struct";
    TypeKind[TypeKind["array"] = -34] = "array";
    TypeKind[TypeKind["empty_block_type"] = -64] = "empty_block_type";
})(TypeKind = exports.TypeKind || (exports.TypeKind = {}));
var Type = /** @class */ (function () {
    function Type(kind, index, depth) {
        if (index === void 0) { index = -1; }
        if (depth === void 0) { depth = -1; }
        if (kind < 0 || (kind === 0 && index >= 0)) {
            // all good
        }
        else {
            throw new Error("invalid type: " + kind + "/" + index + "/" + depth);
        }
        this.kind = kind;
        this.index = index;
        this.depth = depth;
        // Canonicalize (ref any) to (anyref) etc.
        if ((index === -16 /* funcref */ && kind === -20 /* optref */) ||
            (index === -17 /* externref */ && kind === -20 /* optref */) ||
            (index === -18 /* anyref */ && kind === -20 /* optref */) ||
            (index === -19 /* eqref */ && kind === -20 /* optref */) ||
            (index === -22 /* i31ref */ && kind === -21 /* ref */) ||
            (index === -25 /* dataref */ && kind === -21 /* ref */)) {
            this.kind = index;
            this.index = -1;
        }
    }
    // Convenience singletons.
    Type.funcref = new Type(-16 /* funcref */);
    Type.externref = new Type(-17 /* externref */);
    return Type;
}());
exports.Type = Type;
var RelocType;
(function (RelocType) {
    RelocType[RelocType["FunctionIndex_LEB"] = 0] = "FunctionIndex_LEB";
    RelocType[RelocType["TableIndex_SLEB"] = 1] = "TableIndex_SLEB";
    RelocType[RelocType["TableIndex_I32"] = 2] = "TableIndex_I32";
    RelocType[RelocType["GlobalAddr_LEB"] = 3] = "GlobalAddr_LEB";
    RelocType[RelocType["GlobalAddr_SLEB"] = 4] = "GlobalAddr_SLEB";
    RelocType[RelocType["GlobalAddr_I32"] = 5] = "GlobalAddr_I32";
    RelocType[RelocType["TypeIndex_LEB"] = 6] = "TypeIndex_LEB";
    RelocType[RelocType["GlobalIndex_LEB"] = 7] = "GlobalIndex_LEB";
})(RelocType = exports.RelocType || (exports.RelocType = {}));
var LinkingType;
(function (LinkingType) {
    LinkingType[LinkingType["StackPointer"] = 1] = "StackPointer";
})(LinkingType = exports.LinkingType || (exports.LinkingType = {}));
var NameType;
(function (NameType) {
    NameType[NameType["Module"] = 0] = "Module";
    NameType[NameType["Function"] = 1] = "Function";
    NameType[NameType["Local"] = 2] = "Local";
    NameType[NameType["Event"] = 3] = "Event";
    NameType[NameType["Type"] = 4] = "Type";
    NameType[NameType["Table"] = 5] = "Table";
    NameType[NameType["Memory"] = 6] = "Memory";
    NameType[NameType["Global"] = 7] = "Global";
    NameType[NameType["Field"] = 10] = "Field";
})(NameType = exports.NameType || (exports.NameType = {}));
var BinaryReaderState;
(function (BinaryReaderState) {
    BinaryReaderState[BinaryReaderState["ERROR"] = -1] = "ERROR";
    BinaryReaderState[BinaryReaderState["INITIAL"] = 0] = "INITIAL";
    BinaryReaderState[BinaryReaderState["BEGIN_WASM"] = 1] = "BEGIN_WASM";
    BinaryReaderState[BinaryReaderState["END_WASM"] = 2] = "END_WASM";
    BinaryReaderState[BinaryReaderState["BEGIN_SECTION"] = 3] = "BEGIN_SECTION";
    BinaryReaderState[BinaryReaderState["END_SECTION"] = 4] = "END_SECTION";
    BinaryReaderState[BinaryReaderState["SKIPPING_SECTION"] = 5] = "SKIPPING_SECTION";
    BinaryReaderState[BinaryReaderState["READING_SECTION_RAW_DATA"] = 6] = "READING_SECTION_RAW_DATA";
    BinaryReaderState[BinaryReaderState["SECTION_RAW_DATA"] = 7] = "SECTION_RAW_DATA";
    BinaryReaderState[BinaryReaderState["TYPE_SECTION_ENTRY"] = 11] = "TYPE_SECTION_ENTRY";
    BinaryReaderState[BinaryReaderState["IMPORT_SECTION_ENTRY"] = 12] = "IMPORT_SECTION_ENTRY";
    BinaryReaderState[BinaryReaderState["FUNCTION_SECTION_ENTRY"] = 13] = "FUNCTION_SECTION_ENTRY";
    BinaryReaderState[BinaryReaderState["TABLE_SECTION_ENTRY"] = 14] = "TABLE_SECTION_ENTRY";
    BinaryReaderState[BinaryReaderState["MEMORY_SECTION_ENTRY"] = 15] = "MEMORY_SECTION_ENTRY";
    BinaryReaderState[BinaryReaderState["GLOBAL_SECTION_ENTRY"] = 16] = "GLOBAL_SECTION_ENTRY";
    BinaryReaderState[BinaryReaderState["EXPORT_SECTION_ENTRY"] = 17] = "EXPORT_SECTION_ENTRY";
    BinaryReaderState[BinaryReaderState["DATA_SECTION_ENTRY"] = 18] = "DATA_SECTION_ENTRY";
    BinaryReaderState[BinaryReaderState["NAME_SECTION_ENTRY"] = 19] = "NAME_SECTION_ENTRY";
    BinaryReaderState[BinaryReaderState["ELEMENT_SECTION_ENTRY"] = 20] = "ELEMENT_SECTION_ENTRY";
    BinaryReaderState[BinaryReaderState["LINKING_SECTION_ENTRY"] = 21] = "LINKING_SECTION_ENTRY";
    BinaryReaderState[BinaryReaderState["START_SECTION_ENTRY"] = 22] = "START_SECTION_ENTRY";
    BinaryReaderState[BinaryReaderState["EVENT_SECTION_ENTRY"] = 23] = "EVENT_SECTION_ENTRY";
    BinaryReaderState[BinaryReaderState["BEGIN_INIT_EXPRESSION_BODY"] = 25] = "BEGIN_INIT_EXPRESSION_BODY";
    BinaryReaderState[BinaryReaderState["INIT_EXPRESSION_OPERATOR"] = 26] = "INIT_EXPRESSION_OPERATOR";
    BinaryReaderState[BinaryReaderState["END_INIT_EXPRESSION_BODY"] = 27] = "END_INIT_EXPRESSION_BODY";
    BinaryReaderState[BinaryReaderState["BEGIN_FUNCTION_BODY"] = 28] = "BEGIN_FUNCTION_BODY";
    BinaryReaderState[BinaryReaderState["READING_FUNCTION_HEADER"] = 29] = "READING_FUNCTION_HEADER";
    BinaryReaderState[BinaryReaderState["CODE_OPERATOR"] = 30] = "CODE_OPERATOR";
    BinaryReaderState[BinaryReaderState["END_FUNCTION_BODY"] = 31] = "END_FUNCTION_BODY";
    BinaryReaderState[BinaryReaderState["SKIPPING_FUNCTION_BODY"] = 32] = "SKIPPING_FUNCTION_BODY";
    BinaryReaderState[BinaryReaderState["BEGIN_ELEMENT_SECTION_ENTRY"] = 33] = "BEGIN_ELEMENT_SECTION_ENTRY";
    BinaryReaderState[BinaryReaderState["ELEMENT_SECTION_ENTRY_BODY"] = 34] = "ELEMENT_SECTION_ENTRY_BODY";
    BinaryReaderState[BinaryReaderState["END_ELEMENT_SECTION_ENTRY"] = 35] = "END_ELEMENT_SECTION_ENTRY";
    BinaryReaderState[BinaryReaderState["BEGIN_DATA_SECTION_ENTRY"] = 36] = "BEGIN_DATA_SECTION_ENTRY";
    BinaryReaderState[BinaryReaderState["DATA_SECTION_ENTRY_BODY"] = 37] = "DATA_SECTION_ENTRY_BODY";
    BinaryReaderState[BinaryReaderState["END_DATA_SECTION_ENTRY"] = 38] = "END_DATA_SECTION_ENTRY";
    BinaryReaderState[BinaryReaderState["BEGIN_GLOBAL_SECTION_ENTRY"] = 39] = "BEGIN_GLOBAL_SECTION_ENTRY";
    BinaryReaderState[BinaryReaderState["END_GLOBAL_SECTION_ENTRY"] = 40] = "END_GLOBAL_SECTION_ENTRY";
    BinaryReaderState[BinaryReaderState["RELOC_SECTION_HEADER"] = 41] = "RELOC_SECTION_HEADER";
    BinaryReaderState[BinaryReaderState["RELOC_SECTION_ENTRY"] = 42] = "RELOC_SECTION_ENTRY";
    BinaryReaderState[BinaryReaderState["SOURCE_MAPPING_URL"] = 43] = "SOURCE_MAPPING_URL";
    BinaryReaderState[BinaryReaderState["BEGIN_OFFSET_EXPRESSION_BODY"] = 44] = "BEGIN_OFFSET_EXPRESSION_BODY";
    BinaryReaderState[BinaryReaderState["OFFSET_EXPRESSION_OPERATOR"] = 45] = "OFFSET_EXPRESSION_OPERATOR";
    BinaryReaderState[BinaryReaderState["END_OFFSET_EXPRESSION_BODY"] = 46] = "END_OFFSET_EXPRESSION_BODY";
})(BinaryReaderState = exports.BinaryReaderState || (exports.BinaryReaderState = {}));
var DataSegmentType;
(function (DataSegmentType) {
    DataSegmentType[DataSegmentType["Active"] = 0] = "Active";
    DataSegmentType[DataSegmentType["Passive"] = 1] = "Passive";
    DataSegmentType[DataSegmentType["ActiveWithMemoryIndex"] = 2] = "ActiveWithMemoryIndex";
})(DataSegmentType || (DataSegmentType = {}));
function isActiveDataSegmentType(segmentType) {
    switch (segmentType) {
        case 0 /* Active */:
        case 2 /* ActiveWithMemoryIndex */:
            return true;
        default:
            return false;
    }
}
var DataMode;
(function (DataMode) {
    DataMode[DataMode["Active"] = 0] = "Active";
    DataMode[DataMode["Passive"] = 1] = "Passive";
})(DataMode = exports.DataMode || (exports.DataMode = {}));
var ElementSegmentType;
(function (ElementSegmentType) {
    ElementSegmentType[ElementSegmentType["LegacyActiveFuncrefExternval"] = 0] = "LegacyActiveFuncrefExternval";
    ElementSegmentType[ElementSegmentType["PassiveExternval"] = 1] = "PassiveExternval";
    ElementSegmentType[ElementSegmentType["ActiveExternval"] = 2] = "ActiveExternval";
    ElementSegmentType[ElementSegmentType["DeclaredExternval"] = 3] = "DeclaredExternval";
    ElementSegmentType[ElementSegmentType["LegacyActiveFuncrefElemexpr"] = 4] = "LegacyActiveFuncrefElemexpr";
    ElementSegmentType[ElementSegmentType["PassiveElemexpr"] = 5] = "PassiveElemexpr";
    ElementSegmentType[ElementSegmentType["ActiveElemexpr"] = 6] = "ActiveElemexpr";
    ElementSegmentType[ElementSegmentType["DeclaredElemexpr"] = 7] = "DeclaredElemexpr";
})(ElementSegmentType || (ElementSegmentType = {}));
function isActiveElementSegmentType(segmentType) {
    switch (segmentType) {
        case 0 /* LegacyActiveFuncrefExternval */:
        case 2 /* ActiveExternval */:
        case 4 /* LegacyActiveFuncrefElemexpr */:
        case 6 /* ActiveElemexpr */:
            return true;
        default:
            return false;
    }
}
function isExternvalElementSegmentType(segmentType) {
    switch (segmentType) {
        case 0 /* LegacyActiveFuncrefExternval */:
        case 1 /* PassiveExternval */:
        case 2 /* ActiveExternval */:
        case 3 /* DeclaredExternval */:
            return true;
        default:
            return false;
    }
}
var ElementMode;
(function (ElementMode) {
    ElementMode[ElementMode["Active"] = 0] = "Active";
    ElementMode[ElementMode["Passive"] = 1] = "Passive";
    ElementMode[ElementMode["Declarative"] = 2] = "Declarative";
})(ElementMode = exports.ElementMode || (exports.ElementMode = {}));
var DataRange = /** @class */ (function () {
    function DataRange(start, end) {
        this.start = start;
        this.end = end;
    }
    DataRange.prototype.offset = function (delta) {
        this.start += delta;
        this.end += delta;
    };
    return DataRange;
}());
var Int64 = /** @class */ (function () {
    function Int64(data) {
        this._data = data || new Uint8Array(8);
    }
    Int64.prototype.toInt32 = function () {
        return (this._data[0] |
            (this._data[1] << 8) |
            (this._data[2] << 16) |
            (this._data[3] << 24));
    };
    Int64.prototype.toDouble = function () {
        var power = 1;
        var sum;
        if (this._data[7] & 0x80) {
            sum = -1;
            for (var i = 0; i < 8; i++, power *= 256)
                sum -= power * (0xff ^ this._data[i]);
        }
        else {
            sum = 0;
            for (var i = 0; i < 8; i++, power *= 256)
                sum += power * this._data[i];
        }
        return sum;
    };
    Int64.prototype.toString = function () {
        var low = (this._data[0] |
            (this._data[1] << 8) |
            (this._data[2] << 16) |
            (this._data[3] << 24)) >>>
            0;
        var high = (this._data[4] |
            (this._data[5] << 8) |
            (this._data[6] << 16) |
            (this._data[7] << 24)) >>>
            0;
        if (low === 0 && high === 0) {
            return "0";
        }
        var sign = false;
        if (high >> 31) {
            high = 4294967296 - high;
            if (low > 0) {
                high--;
                low = 4294967296 - low;
            }
            sign = true;
        }
        var buf = [];
        while (high > 0) {
            var t = (high % 10) * 4294967296 + low;
            high = Math.floor(high / 10);
            buf.unshift((t % 10).toString());
            low = Math.floor(t / 10);
        }
        while (low > 0) {
            buf.unshift((low % 10).toString());
            low = Math.floor(low / 10);
        }
        if (sign)
            buf.unshift("-");
        return buf.join("");
    };
    Object.defineProperty(Int64.prototype, "data", {
        get: function () {
            return this._data;
        },
        enumerable: false,
        configurable: true
    });
    return Int64;
}());
exports.Int64 = Int64;
var BinaryReader = /** @class */ (function () {
    function BinaryReader() {
        this._data = null;
        this._pos = 0;
        this._length = 0;
        this._eof = false;
        this.state = 0 /* INITIAL */;
        this.result = null;
        this.error = null;
        this._sectionEntriesLeft = 0;
        this._sectionId = -1 /* Unknown */;
        this._sectionRange = null;
        this._functionRange = null;
        this._segmentType = 0;
        this._segmentEntriesLeft = 0;
    }
    Object.defineProperty(BinaryReader.prototype, "data", {
        get: function () {
            return this._data;
        },
        enumerable: false,
        configurable: true
    });
    Object.defineProperty(BinaryReader.prototype, "position", {
        get: function () {
            return this._pos;
        },
        enumerable: false,
        configurable: true
    });
    Object.defineProperty(BinaryReader.prototype, "length", {
        get: function () {
            return this._length;
        },
        enumerable: false,
        configurable: true
    });
    BinaryReader.prototype.setData = function (buffer, pos, length, eof) {
        var posDelta = pos - this._pos;
        this._data = new Uint8Array(buffer);
        this._pos = pos;
        this._length = length;
        this._eof = eof === undefined ? true : eof;
        if (this._sectionRange)
            this._sectionRange.offset(posDelta);
        if (this._functionRange)
            this._functionRange.offset(posDelta);
    };
    BinaryReader.prototype.hasBytes = function (n) {
        return this._pos + n <= this._length;
    };
    BinaryReader.prototype.hasMoreBytes = function () {
        return this.hasBytes(1);
    };
    BinaryReader.prototype.readUint8 = function () {
        return this._data[this._pos++];
    };
    BinaryReader.prototype.readInt32 = function () {
        var b1 = this._data[this._pos++];
        var b2 = this._data[this._pos++];
        var b3 = this._data[this._pos++];
        var b4 = this._data[this._pos++];
        return b1 | (b2 << 8) | (b3 << 16) | (b4 << 24);
    };
    BinaryReader.prototype.readUint32 = function () {
        return this.readInt32();
    };
    BinaryReader.prototype.peekInt32 = function () {
        var b1 = this._data[this._pos];
        var b2 = this._data[this._pos + 1];
        var b3 = this._data[this._pos + 2];
        var b4 = this._data[this._pos + 3];
        return b1 | (b2 << 8) | (b3 << 16) | (b4 << 24);
    };
    BinaryReader.prototype.hasVarIntBytes = function () {
        var pos = this._pos;
        while (pos < this._length) {
            if ((this._data[pos++] & 0x80) == 0)
                return true;
        }
        return false;
    };
    BinaryReader.prototype.readVarUint1 = function () {
        return this.readUint8();
    };
    BinaryReader.prototype.readVarInt7 = function () {
        return (this.readUint8() << 25) >> 25;
    };
    BinaryReader.prototype.readVarUint7 = function () {
        return this.readUint8();
    };
    BinaryReader.prototype.readVarInt32 = function () {
        var result = 0;
        var shift = 0;
        while (true) {
            var byte = this.readUint8();
            result |= (byte & 0x7f) << shift;
            shift += 7;
            if ((byte & 0x80) === 0)
                break;
        }
        if (shift >= 32)
            return result;
        var ashift = 32 - shift;
        return (result << ashift) >> ashift;
    };
    BinaryReader.prototype.readVarUint32 = function () {
        var result = 0;
        var shift = 0;
        while (true) {
            var byte = this.readUint8();
            result |= (byte & 0x7f) << shift;
            shift += 7;
            if ((byte & 0x80) === 0)
                break;
        }
        return result;
    };
    BinaryReader.prototype.readVarInt64 = function () {
        var result = new Uint8Array(8);
        var i = 0;
        var c = 0;
        var shift = 0;
        while (true) {
            var byte = this.readUint8();
            c |= (byte & 0x7f) << shift;
            shift += 7;
            if (shift > 8) {
                result[i++] = c & 0xff;
                c >>= 8;
                shift -= 8;
            }
            if ((byte & 0x80) === 0)
                break;
        }
        var ashift = 32 - shift;
        c = (c << ashift) >> ashift;
        while (i < 8) {
            result[i++] = c & 0xff;
            c >>= 8;
        }
        return new Int64(result);
    };
    // Reads any "s33" (signed 33-bit integer) value correctly; no guarantees
    // outside that range.
    BinaryReader.prototype.readHeapType = function () {
        var result = 0;
        var shift = 0;
        var byte;
        while (true) {
            byte = this.readUint8();
            if (shift === 28) {
                var signed = (byte << 25) >> 25;
                return signed * Math.pow(2, 28) + result;
            }
            result |= (byte & 0x7f) << shift;
            shift += 7;
            if ((byte & 0x80) === 0)
                break;
        }
        shift = 32 - shift;
        return (result << shift) >> shift;
    };
    BinaryReader.prototype.readTypeInternal = function (kind) {
        if (kind === -21 /* ref */ ||
            kind === -20 /* optref */ ||
            kind === -24 /* rtt */) {
            var index = this.readHeapType();
            return new Type(kind, index);
        }
        if (kind === -23 /* rtt_d */) {
            var index = this.readHeapType();
            var depth = this.readVarUint32();
            return new Type(kind, index, depth);
        }
        return new Type(kind);
    };
    BinaryReader.prototype.readType = function () {
        var kind = this.readVarInt7();
        return this.readTypeInternal(kind);
    };
    BinaryReader.prototype.readBlockType = function () {
        var block_type = this.readHeapType();
        if (block_type < 0) {
            return this.readTypeInternal(block_type);
        }
        var func_index = block_type;
        return new Type(0 /* unspecified */, func_index);
    };
    BinaryReader.prototype.readStringBytes = function () {
        var length = this.readVarUint32() >>> 0;
        return this.readBytes(length);
    };
    BinaryReader.prototype.readBytes = function (length) {
        var result = this._data.subarray(this._pos, this._pos + length);
        this._pos += length;
        return new Uint8Array(result); // making a clone of the data
    };
    BinaryReader.prototype.skipBytes = function (length) {
        this._pos += length;
    };
    BinaryReader.prototype.hasStringBytes = function () {
        if (!this.hasVarIntBytes())
            return false;
        var pos = this._pos;
        var length = this.readVarUint32() >>> 0;
        var result = this.hasBytes(length);
        this._pos = pos;
        return result;
    };
    BinaryReader.prototype.hasSectionPayload = function () {
        return this.hasBytes(this._sectionRange.end - this._pos);
    };
    BinaryReader.prototype.readFuncType = function () {
        var paramCount = this.readVarUint32() >>> 0;
        var paramTypes = new Array(paramCount);
        for (var i = 0; i < paramCount; i++)
            paramTypes[i] = this.readType();
        var returnCount = this.readVarUint1();
        var returnTypes = new Array(returnCount);
        for (var i = 0; i < returnCount; i++)
            returnTypes[i] = this.readType();
        return {
            form: -32 /* func */,
            params: paramTypes,
            returns: returnTypes,
        };
    };
    BinaryReader.prototype.readStructType = function () {
        var fieldCount = this.readVarUint32() >>> 0;
        var fieldTypes = new Array(fieldCount);
        var fieldMutabilities = new Array(fieldCount);
        for (var i = 0; i < fieldCount; i++) {
            fieldTypes[i] = this.readType();
            fieldMutabilities[i] = !!this.readVarUint1();
        }
        return {
            form: -33 /* struct */,
            fields: fieldTypes,
            mutabilities: fieldMutabilities,
        };
    };
    BinaryReader.prototype.readArrayType = function () {
        var elementType = this.readType();
        var mutability = !!this.readVarUint1();
        return {
            form: -34 /* array */,
            elementType: elementType,
            mutability: mutability,
        };
    };
    BinaryReader.prototype.readResizableLimits = function (maxPresent) {
        var initial = this.readVarUint32() >>> 0;
        var maximum;
        if (maxPresent) {
            maximum = this.readVarUint32() >>> 0;
        }
        return { initial: initial, maximum: maximum };
    };
    BinaryReader.prototype.readTableType = function () {
        var elementType = this.readType();
        var flags = this.readVarUint32() >>> 0;
        var limits = this.readResizableLimits(!!(flags & 0x01));
        return { elementType: elementType, limits: limits };
    };
    BinaryReader.prototype.readMemoryType = function () {
        var flags = this.readVarUint32() >>> 0;
        var shared = !!(flags & 0x02);
        return {
            limits: this.readResizableLimits(!!(flags & 0x01)),
            shared: shared,
        };
    };
    BinaryReader.prototype.readGlobalType = function () {
        if (!this.hasVarIntBytes()) {
            return null;
        }
        var pos = this._pos;
        var contentType = this.readType();
        if (!this.hasVarIntBytes()) {
            this._pos = pos;
            return null;
        }
        var mutability = this.readVarUint1();
        return { contentType: contentType, mutability: mutability };
    };
    BinaryReader.prototype.readEventType = function () {
        var attribute = this.readVarUint32() >>> 0;
        var typeIndex = this.readVarUint32() >>> 0;
        return {
            attribute: attribute,
            typeIndex: typeIndex,
        };
    };
    BinaryReader.prototype.readTypeEntry = function () {
        if (this._sectionEntriesLeft === 0) {
            this.skipSection();
            return this.read();
        }
        this.state = 11 /* TYPE_SECTION_ENTRY */;
        var form = this.readVarInt7();
        switch (form) {
            case -32 /* func */:
                this.result = this.readFuncType();
                break;
            case -33 /* struct */:
                this.result = this.readStructType();
                break;
            case -34 /* array */:
                this.result = this.readArrayType();
                break;
            default:
                throw new Error("Unknown type kind: " + form);
        }
        this._sectionEntriesLeft--;
        return true;
    };
    BinaryReader.prototype.readImportEntry = function () {
        if (this._sectionEntriesLeft === 0) {
            this.skipSection();
            return this.read();
        }
        this.state = 12 /* IMPORT_SECTION_ENTRY */;
        var module = this.readStringBytes();
        var field = this.readStringBytes();
        var kind = this.readUint8();
        var funcTypeIndex;
        var type;
        switch (kind) {
            case 0 /* Function */:
                funcTypeIndex = this.readVarUint32() >>> 0;
                break;
            case 1 /* Table */:
                type = this.readTableType();
                break;
            case 2 /* Memory */:
                type = this.readMemoryType();
                break;
            case 3 /* Global */:
                type = this.readGlobalType();
                break;
            case 4 /* Event */:
                type = this.readEventType();
                break;
        }
        this.result = {
            module: module,
            field: field,
            kind: kind,
            funcTypeIndex: funcTypeIndex,
            type: type,
        };
        this._sectionEntriesLeft--;
        return true;
    };
    BinaryReader.prototype.readExportEntry = function () {
        if (this._sectionEntriesLeft === 0) {
            this.skipSection();
            return this.read();
        }
        var field = this.readStringBytes();
        var kind = this.readUint8();
        var index = this.readVarUint32() >>> 0;
        this.state = 17 /* EXPORT_SECTION_ENTRY */;
        this.result = { field: field, kind: kind, index: index };
        this._sectionEntriesLeft--;
        return true;
    };
    BinaryReader.prototype.readFunctionEntry = function () {
        if (this._sectionEntriesLeft === 0) {
            this.skipSection();
            return this.read();
        }
        var typeIndex = this.readVarUint32() >>> 0;
        this.state = 13 /* FUNCTION_SECTION_ENTRY */;
        this.result = { typeIndex: typeIndex };
        this._sectionEntriesLeft--;
        return true;
    };
    BinaryReader.prototype.readTableEntry = function () {
        if (this._sectionEntriesLeft === 0) {
            this.skipSection();
            return this.read();
        }
        this.state = 14 /* TABLE_SECTION_ENTRY */;
        this.result = this.readTableType();
        this._sectionEntriesLeft--;
        return true;
    };
    BinaryReader.prototype.readMemoryEntry = function () {
        if (this._sectionEntriesLeft === 0) {
            this.skipSection();
            return this.read();
        }
        this.state = 15 /* MEMORY_SECTION_ENTRY */;
        this.result = this.readMemoryType();
        this._sectionEntriesLeft--;
        return true;
    };
    BinaryReader.prototype.readEventEntry = function () {
        if (this._sectionEntriesLeft === 0) {
            this.skipSection();
            return this.read();
        }
        this.state = 23 /* EVENT_SECTION_ENTRY */;
        this.result = this.readEventType();
        this._sectionEntriesLeft--;
        return true;
    };
    BinaryReader.prototype.readGlobalEntry = function () {
        if (this._sectionEntriesLeft === 0) {
            this.skipSection();
            return this.read();
        }
        var globalType = this.readGlobalType();
        if (!globalType) {
            this.state = 16 /* GLOBAL_SECTION_ENTRY */;
            return false;
        }
        this.state = 39 /* BEGIN_GLOBAL_SECTION_ENTRY */;
        this.result = {
            type: globalType,
        };
        this._sectionEntriesLeft--;
        return true;
    };
    BinaryReader.prototype.readElementEntry = function () {
        if (this._sectionEntriesLeft === 0) {
            this.skipSection();
            return this.read();
        }
        var pos = this._pos;
        if (!this.hasMoreBytes()) {
            this.state = 20 /* ELEMENT_SECTION_ENTRY */;
            return false;
        }
        var segmentType = this.readUint8();
        var mode, tableIndex;
        switch (segmentType) {
            case 0 /* LegacyActiveFuncrefExternval */:
            case 4 /* LegacyActiveFuncrefElemexpr */:
                mode = 0 /* Active */;
                tableIndex = 0;
                break;
            case 1 /* PassiveExternval */:
            case 5 /* PassiveElemexpr */:
                mode = 1 /* Passive */;
                break;
            case 2 /* ActiveExternval */:
            case 6 /* ActiveElemexpr */:
                mode = 0 /* Active */;
                if (!this.hasVarIntBytes()) {
                    this.state = 20 /* ELEMENT_SECTION_ENTRY */;
                    this._pos = pos;
                    return false;
                }
                tableIndex = this.readVarUint32();
                break;
            case 3 /* DeclaredExternval */:
            case 7 /* DeclaredElemexpr */:
                mode = 2 /* Declarative */;
                break;
            default:
                throw new Error("Unsupported element segment type " + segmentType);
        }
        this.state = 33 /* BEGIN_ELEMENT_SECTION_ENTRY */;
        this.result = { mode: mode, tableIndex: tableIndex };
        this._sectionEntriesLeft--;
        this._segmentType = segmentType;
        return true;
    };
    BinaryReader.prototype.readElementEntryBody = function () {
        var elementType = Type.funcref;
        switch (this._segmentType) {
            case 1 /* PassiveExternval */:
            case 2 /* ActiveExternval */:
            case 3 /* DeclaredExternval */:
                if (!this.hasMoreBytes())
                    return false;
                // We just skip the 0x00 byte, the `elemkind` byte
                // is reserved for future versions of WebAssembly.
                this.skipBytes(1);
                break;
            case 5 /* PassiveElemexpr */:
            case 6 /* ActiveElemexpr */:
            case 7 /* DeclaredElemexpr */:
                if (!this.hasMoreBytes())
                    return false;
                elementType = this.readType();
                break;
            case 0 /* LegacyActiveFuncrefExternval */:
            case 4 /* LegacyActiveFuncrefElemexpr */:
                // The element type is implicitly `funcref`.
                break;
            default:
                throw new Error("Unsupported element segment type " + this._segmentType);
        }
        this.state = 34 /* ELEMENT_SECTION_ENTRY_BODY */;
        this.result = { elementType: elementType };
        return true;
    };
    BinaryReader.prototype.readDataEntry = function () {
        if (this._sectionEntriesLeft === 0) {
            this.skipSection();
            return this.read();
        }
        var pos = this._pos;
        if (!this.hasVarIntBytes()) {
            this.state = 18 /* DATA_SECTION_ENTRY */;
            return false;
        }
        var segmentType = this.readVarUint32();
        var mode, memoryIndex;
        switch (segmentType) {
            case 0 /* Active */:
                mode = 0 /* Active */;
                memoryIndex = 0;
                break;
            case 1 /* Passive */:
                mode = 1 /* Passive */;
                break;
            case 2 /* ActiveWithMemoryIndex */:
                mode = 0 /* Active */;
                if (!this.hasVarIntBytes()) {
                    this._pos = pos;
                    this.state = 18 /* DATA_SECTION_ENTRY */;
                    return false;
                }
                memoryIndex = this.readVarUint32();
                break;
            default:
                throw new Error("Unsupported data segment type " + segmentType);
        }
        this.state = 36 /* BEGIN_DATA_SECTION_ENTRY */;
        this.result = { mode: mode, memoryIndex: memoryIndex };
        this._sectionEntriesLeft--;
        this._segmentType = segmentType;
        return true;
    };
    BinaryReader.prototype.readDataEntryBody = function () {
        if (!this.hasStringBytes()) {
            return false;
        }
        this.state = 37 /* DATA_SECTION_ENTRY_BODY */;
        this.result = {
            data: this.readStringBytes(),
        };
        return true;
    };
    BinaryReader.prototype.readInitExpressionBody = function () {
        this.state = 25 /* BEGIN_INIT_EXPRESSION_BODY */;
        this.result = null;
        return true;
    };
    BinaryReader.prototype.readOffsetExpressionBody = function () {
        this.state = 44 /* BEGIN_OFFSET_EXPRESSION_BODY */;
        this.result = null;
        return true;
    };
    BinaryReader.prototype.readMemoryImmediate = function () {
        var flags = this.readVarUint32() >>> 0;
        var offset = this.readVarUint32() >>> 0;
        return { flags: flags, offset: offset };
    };
    BinaryReader.prototype.readNameMap = function () {
        var count = this.readVarUint32();
        var result = [];
        for (var i = 0; i < count; i++) {
            var index = this.readVarUint32();
            var name = this.readStringBytes();
            result.push({ index: index, name: name });
        }
        return result;
    };
    BinaryReader.prototype.readNameEntry = function () {
        var pos = this._pos;
        if (pos >= this._sectionRange.end) {
            this.skipSection();
            return this.read();
        }
        if (!this.hasVarIntBytes())
            return false;
        var type = this.readVarUint7();
        if (!this.hasVarIntBytes()) {
            this._pos = pos;
            return false;
        }
        var payloadLength = this.readVarUint32();
        if (!this.hasBytes(payloadLength)) {
            this._pos = pos;
            return false;
        }
        var result;
        switch (type) {
            case 0 /* Module */:
                result = {
                    type: type,
                    moduleName: this.readStringBytes(),
                };
                break;
            case 1 /* Function */:
            case 3 /* Event */:
            case 4 /* Type */:
            case 5 /* Table */:
            case 6 /* Memory */:
            case 7 /* Global */:
                result = {
                    type: type,
                    names: this.readNameMap(),
                };
                break;
            case 2 /* Local */:
                var funcsLength = this.readVarUint32();
                var funcs = [];
                for (var i = 0; i < funcsLength; i++) {
                    var funcIndex = this.readVarUint32();
                    funcs.push({
                        index: funcIndex,
                        locals: this.readNameMap(),
                    });
                }
                result = {
                    type: type,
                    funcs: funcs,
                };
                break;
            case 10 /* Field */:
                var typesLength = this.readVarUint32();
                var types = [];
                for (var i = 0; i < typesLength; i++) {
                    var fieldIndex = this.readVarUint32();
                    types.push({
                        index: fieldIndex,
                        fields: this.readNameMap(),
                    });
                }
                result = {
                    type: type,
                    types: types,
                };
                break;
            default:
                // Skip this unknown name subsection (as per specification,
                // custom section errors shouldn't cause Wasm parsing to fail).
                this.skipBytes(payloadLength);
                return this.read();
        }
        this.state = 19 /* NAME_SECTION_ENTRY */;
        this.result = result;
        return true;
    };
    BinaryReader.prototype.readRelocHeader = function () {
        // See https://github.com/WebAssembly/tool-conventions/blob/master/Linking.md
        if (!this.hasVarIntBytes()) {
            return false;
        }
        var pos = this._pos;
        var sectionId = this.readVarUint7();
        var sectionName;
        if (sectionId === 0 /* Custom */) {
            if (!this.hasStringBytes()) {
                this._pos = pos;
                return false;
            }
            sectionName = this.readStringBytes();
        }
        this.state = 41 /* RELOC_SECTION_HEADER */;
        this.result = {
            id: sectionId,
            name: sectionName,
        };
        return true;
    };
    BinaryReader.prototype.readLinkingEntry = function () {
        if (this._sectionEntriesLeft === 0) {
            this.skipSection();
            return this.read();
        }
        if (!this.hasVarIntBytes())
            return false;
        var pos = this._pos;
        var type = this.readVarUint32() >>> 0;
        var index;
        switch (type) {
            case 1 /* StackPointer */:
                if (!this.hasVarIntBytes()) {
                    this._pos = pos;
                    return false;
                }
                index = this.readVarUint32();
                break;
            default:
                this.error = new Error("Bad linking type: " + type);
                this.state = -1 /* ERROR */;
                return true;
        }
        this.state = 21 /* LINKING_SECTION_ENTRY */;
        this.result = { type: type, index: index };
        this._sectionEntriesLeft--;
        return true;
    };
    BinaryReader.prototype.readSourceMappingURL = function () {
        if (!this.hasStringBytes())
            return false;
        var url = this.readStringBytes();
        this.state = 43 /* SOURCE_MAPPING_URL */;
        this.result = { url: url };
        return true;
    };
    BinaryReader.prototype.readRelocEntry = function () {
        if (this._sectionEntriesLeft === 0) {
            this.skipSection();
            return this.read();
        }
        if (!this.hasVarIntBytes())
            return false;
        var pos = this._pos;
        var type = this.readVarUint7();
        if (!this.hasVarIntBytes()) {
            this._pos = pos;
            return false;
        }
        var offset = this.readVarUint32();
        if (!this.hasVarIntBytes()) {
            this._pos = pos;
            return false;
        }
        var index = this.readVarUint32();
        var addend;
        switch (type) {
            case 0 /* FunctionIndex_LEB */:
            case 1 /* TableIndex_SLEB */:
            case 2 /* TableIndex_I32 */:
            case 6 /* TypeIndex_LEB */:
            case 7 /* GlobalIndex_LEB */:
                break;
            case 3 /* GlobalAddr_LEB */:
            case 4 /* GlobalAddr_SLEB */:
            case 5 /* GlobalAddr_I32 */:
                if (!this.hasVarIntBytes()) {
                    this._pos = pos;
                    return false;
                }
                addend = this.readVarUint32();
                break;
            default:
                this.error = new Error("Bad relocation type: " + type);
                this.state = -1 /* ERROR */;
                return true;
        }
        this.state = 42 /* RELOC_SECTION_ENTRY */;
        this.result = {
            type: type,
            offset: offset,
            index: index,
            addend: addend,
        };
        this._sectionEntriesLeft--;
        return true;
    };
    BinaryReader.prototype.readCodeOperator_0xfb = function () {
        // The longest instructions have: 2 bytes opcode, 5 bytes type index,
        // 5 bytes field index.
        var MAX_CODE_OPERATOR_0XFB_SIZE = 12;
        if (!this._eof && !this.hasBytes(MAX_CODE_OPERATOR_0XFB_SIZE)) {
            return false;
        }
        var code, brDepth, refType, fieldIndex;
        code = this._data[this._pos++] | 0xfb00;
        switch (code) {
            case 64322 /* br_on_cast */:
            case 64352 /* br_on_func */:
            case 64353 /* br_on_data */:
            case 64354 /* br_on_i31 */:
                brDepth = this.readVarUint32() >>> 0;
                break;
            case 64275 /* array_get */:
            case 64276 /* array_get_s */:
            case 64277 /* array_get_u */:
            case 64279 /* array_len */:
            case 64278 /* array_set */:
            case 64273 /* array_new_with_rtt */:
            case 64274 /* array_new_default_with_rtt */:
            case 64257 /* struct_new_with_rtt */:
            case 64258 /* struct_new_default_with_rtt */:
            case 64304 /* rtt_canon */:
            case 64305 /* rtt_sub */:
                refType = this.readHeapType();
                break;
            case 64259 /* struct_get */:
            case 64260 /* struct_get_s */:
            case 64261 /* struct_get_u */:
            case 64262 /* struct_set */:
                refType = this.readHeapType();
                fieldIndex = this.readVarUint32();
                break;
            case 64336 /* ref_is_func */:
            case 64337 /* ref_is_data */:
            case 64338 /* ref_is_i31 */:
            case 64344 /* ref_as_func */:
            case 64345 /* ref_as_data */:
            case 64346 /* ref_as_i31 */:
            case 64320 /* ref_test */:
            case 64321 /* ref_cast */:
            case 64288 /* i31_new */:
            case 64289 /* i31_get_s */:
            case 64290 /* i31_get_u */:
                break;
            default:
                this.error = new Error("Unknown operator: 0x" + code.toString(16).padStart(4, "0"));
                this.state = -1 /* ERROR */;
                return true;
        }
        this.result = {
            code: code,
            blockType: undefined,
            refType: refType,
            brDepth: brDepth,
            brTable: undefined,
            tableIndex: undefined,
            funcIndex: undefined,
            typeIndex: undefined,
            localIndex: undefined,
            globalIndex: undefined,
            fieldIndex: fieldIndex,
            memoryAddress: undefined,
            literal: undefined,
            segmentIndex: undefined,
            destinationIndex: undefined,
            lines: undefined,
            lineIndex: undefined,
        };
        return true;
    };
    BinaryReader.prototype.readCodeOperator_0xfc = function () {
        if (!this.hasVarIntBytes()) {
            return false;
        }
        var code = this.readVarUint32() | 0xfc00;
        var reserved, segmentIndex, destinationIndex, tableIndex;
        switch (code) {
            case 64512 /* i32_trunc_sat_f32_s */:
            case 64513 /* i32_trunc_sat_f32_u */:
            case 64514 /* i32_trunc_sat_f64_s */:
            case 64515 /* i32_trunc_sat_f64_u */:
            case 64516 /* i64_trunc_sat_f32_s */:
            case 64517 /* i64_trunc_sat_f32_u */:
            case 64518 /* i64_trunc_sat_f64_s */:
            case 64519 /* i64_trunc_sat_f64_u */:
                break;
            case 64522 /* memory_copy */:
                // Currently memory index must be zero.
                reserved = this.readVarUint1();
                reserved = this.readVarUint1();
                break;
            case 64523 /* memory_fill */:
                reserved = this.readVarUint1();
                break;
            case 64524 /* table_init */:
                segmentIndex = this.readVarUint32() >>> 0;
                tableIndex = this.readVarUint32() >>> 0;
                break;
            case 64526 /* table_copy */:
                tableIndex = this.readVarUint32() >>> 0;
                destinationIndex = this.readVarUint32() >>> 0;
                break;
            case 64527 /* table_grow */:
            case 64528 /* table_size */:
            case 64529 /* table_fill */:
                tableIndex = this.readVarUint32() >>> 0;
                break;
            case 64520 /* memory_init */:
                segmentIndex = this.readVarUint32() >>> 0;
                reserved = this.readVarUint1();
                break;
            case 64521 /* data_drop */:
            case 64525 /* elem_drop */:
                segmentIndex = this.readVarUint32() >>> 0;
                break;
            default:
                this.error = new Error("Unknown operator: 0x" + code.toString(16).padStart(4, "0"));
                this.state = -1 /* ERROR */;
                return true;
        }
        this.result = {
            code: code,
            blockType: undefined,
            refType: undefined,
            brDepth: undefined,
            brTable: undefined,
            funcIndex: undefined,
            typeIndex: undefined,
            tableIndex: tableIndex,
            localIndex: undefined,
            globalIndex: undefined,
            fieldIndex: undefined,
            memoryAddress: undefined,
            literal: undefined,
            segmentIndex: segmentIndex,
            destinationIndex: destinationIndex,
            lines: undefined,
            lineIndex: undefined,
        };
        return true;
    };
    BinaryReader.prototype.readCodeOperator_0xfd = function () {
        var MAX_CODE_OPERATOR_0XFD_SIZE = 17;
        var pos = this._pos;
        if (!this._eof && pos + MAX_CODE_OPERATOR_0XFD_SIZE > this._length) {
            return false;
        }
        if (!this.hasVarIntBytes()) {
            return false;
        }
        var code = this.readVarUint32() | 0xfd00;
        var memoryAddress;
        var literal;
        var lineIndex;
        var lines;
        switch (code) {
            case 64768 /* v128_load */:
            case 64769 /* i16x8_load8x8_s */:
            case 64770 /* i16x8_load8x8_u */:
            case 64771 /* i32x4_load16x4_s */:
            case 64772 /* i32x4_load16x4_u */:
            case 64773 /* i64x2_load32x2_s */:
            case 64774 /* i64x2_load32x2_u */:
            case 64775 /* v8x16_load_splat */:
            case 64776 /* v16x8_load_splat */:
            case 64777 /* v32x4_load_splat */:
            case 64778 /* v64x2_load_splat */:
            case 64779 /* v128_store */:
            case 64860 /* v128_load32_zero */:
            case 64861 /* v128_load64_zero */:
                memoryAddress = this.readMemoryImmediate();
                break;
            case 64780 /* v128_const */:
                literal = this.readBytes(16);
                break;
            case 64781 /* i8x16_shuffle */:
                lines = new Uint8Array(16);
                for (var i = 0; i < lines.length; i++) {
                    lines[i] = this.readUint8();
                }
                break;
            case 64789 /* i8x16_extract_lane_s */:
            case 64790 /* i8x16_extract_lane_u */:
            case 64791 /* i8x16_replace_lane */:
            case 64792 /* i16x8_extract_lane_s */:
            case 64793 /* i16x8_extract_lane_u */:
            case 64794 /* i16x8_replace_lane */:
            case 64795 /* i32x4_extract_lane */:
            case 64796 /* i32x4_replace_lane */:
            case 64797 /* i64x2_extract_lane */:
            case 64798 /* i64x2_replace_lane */:
            case 64799 /* f32x4_extract_lane */:
            case 64800 /* f32x4_replace_lane */:
            case 64801 /* f64x2_extract_lane */:
            case 64802 /* f64x2_replace_lane */:
                lineIndex = this.readUint8();
                break;
            case 64782 /* i8x16_swizzle */:
            case 64783 /* i8x16_splat */:
            case 64784 /* i16x8_splat */:
            case 64785 /* i32x4_splat */:
            case 64786 /* i64x2_splat */:
            case 64787 /* f32x4_splat */:
            case 64788 /* f64x2_splat */:
            case 64803 /* i8x16_eq */:
            case 64804 /* i8x16_ne */:
            case 64805 /* i8x16_lt_s */:
            case 64806 /* i8x16_lt_u */:
            case 64807 /* i8x16_gt_s */:
            case 64808 /* i8x16_gt_u */:
            case 64809 /* i8x16_le_s */:
            case 64810 /* i8x16_le_u */:
            case 64811 /* i8x16_ge_s */:
            case 64812 /* i8x16_ge_u */:
            case 64813 /* i16x8_eq */:
            case 64814 /* i16x8_ne */:
            case 64815 /* i16x8_lt_s */:
            case 64816 /* i16x8_lt_u */:
            case 64817 /* i16x8_gt_s */:
            case 64818 /* i16x8_gt_u */:
            case 64819 /* i16x8_le_s */:
            case 64820 /* i16x8_le_u */:
            case 64821 /* i16x8_ge_s */:
            case 64822 /* i16x8_ge_u */:
            case 64823 /* i32x4_eq */:
            case 64824 /* i32x4_ne */:
            case 64825 /* i32x4_lt_s */:
            case 64826 /* i32x4_lt_u */:
            case 64827 /* i32x4_gt_s */:
            case 64828 /* i32x4_gt_u */:
            case 64829 /* i32x4_le_s */:
            case 64830 /* i32x4_le_u */:
            case 64831 /* i32x4_ge_s */:
            case 64832 /* i32x4_ge_u */:
            case 64833 /* f32x4_eq */:
            case 64834 /* f32x4_ne */:
            case 64835 /* f32x4_lt */:
            case 64836 /* f32x4_gt */:
            case 64837 /* f32x4_le */:
            case 64838 /* f32x4_ge */:
            case 64839 /* f64x2_eq */:
            case 64840 /* f64x2_ne */:
            case 64841 /* f64x2_lt */:
            case 64842 /* f64x2_gt */:
            case 64843 /* f64x2_le */:
            case 64844 /* f64x2_ge */:
            case 64845 /* v128_not */:
            case 64846 /* v128_and */:
            case 64847 /* v128_andnot */:
            case 64848 /* v128_or */:
            case 64849 /* v128_xor */:
            case 64850 /* v128_bitselect */:
            case 64851 /* v128_any_true */:
            case 64862 /* f32x4_demote_f64x2_zero */:
            case 64863 /* f64x2_promote_low_f32x4 */:
            case 64864 /* i8x16_abs */:
            case 64865 /* i8x16_neg */:
            case 64866 /* i8x16_popcnt */:
            case 64867 /* i8x16_all_true */:
            case 64868 /* i8x16_bitmask */:
            case 64869 /* i8x16_narrow_i16x8_s */:
            case 64870 /* i8x16_narrow_i16x8_u */:
            case 103 /* f32x4_ceil */:
            case 104 /* f32x4_floor */:
            case 105 /* f32x4_trunc */:
            case 106 /* f32x4_nearest */:
            case 64875 /* i8x16_shl */:
            case 64876 /* i8x16_shr_s */:
            case 64877 /* i8x16_shr_u */:
            case 64878 /* i8x16_add */:
            case 64879 /* i8x16_add_sat_s */:
            case 64880 /* i8x16_add_sat_u */:
            case 64881 /* i8x16_sub */:
            case 64882 /* i8x16_sub_sat_s */:
            case 64883 /* i8x16_sub_sat_u */:
            case 64884 /* f64x2_ceil */:
            case 64885 /* f64x2_floor */:
            case 64886 /* i8x16_min_s */:
            case 64887 /* i8x16_min_u */:
            case 64888 /* i8x16_max_s */:
            case 64889 /* i8x16_max_u */:
            case 64890 /* f64x2_trunc */:
            case 64891 /* i8x16_avgr_u */:
            case 64892 /* i16x8_extadd_pairwise_i8x16_s */:
            case 64893 /* i16x8_extadd_pairwise_i8x16_u */:
            case 64894 /* i32x4_extadd_pairwise_i16x8_s */:
            case 64895 /* i32x4_extadd_pairwise_i16x8_u */:
            case 64896 /* i16x8_abs */:
            case 64897 /* i16x8_neg */:
            case 64898 /* i16x8_q15mulr_sat_s */:
            case 64899 /* i16x8_all_true */:
            case 64900 /* i16x8_bitmask */:
            case 64901 /* i16x8_narrow_i32x4_s */:
            case 64902 /* i16x8_narrow_i32x4_u */:
            case 64903 /* i16x8_extend_low_i8x16_s */:
            case 64904 /* i16x8_extend_high_i8x16_s */:
            case 64905 /* i16x8_extend_low_i8x16_u */:
            case 64906 /* i16x8_extend_high_i8x16_u */:
            case 64907 /* i16x8_shl */:
            case 64908 /* i16x8_shr_s */:
            case 64909 /* i16x8_shr_u */:
            case 64910 /* i16x8_add */:
            case 64911 /* i16x8_add_sat_s */:
            case 64912 /* i16x8_add_sat_u */:
            case 64913 /* i16x8_sub */:
            case 64914 /* i16x8_sub_sat_s */:
            case 64915 /* i16x8_sub_sat_u */:
            case 64916 /* f64x2_nearest */:
            case 64917 /* i16x8_mul */:
            case 64918 /* i16x8_min_s */:
            case 64919 /* i16x8_min_u */:
            case 64920 /* i16x8_max_s */:
            case 64921 /* i16x8_max_u */:
            case 64923 /* i16x8_avgr_u */:
            case 64924 /* i16x8_extmul_low_i8x16_s */:
            case 64925 /* i16x8_extmul_high_i8x16_s */:
            case 64926 /* i16x8_extmul_low_i8x16_u */:
            case 64927 /* i16x8_extmul_high_i8x16_u */:
            case 64928 /* i32x4_abs */:
            case 64929 /* i32x4_neg */:
            case 64931 /* i32x4_all_true */:
            case 64932 /* i32x4_bitmask */:
            case 64935 /* i32x4_extend_low_i16x8_s */:
            case 64936 /* i32x4_extend_high_i16x8_s */:
            case 64937 /* i32x4_extend_low_i16x8_u */:
            case 64938 /* i32x4_extend_high_i16x8_u */:
            case 64939 /* i32x4_shl */:
            case 64940 /* i32x4_shr_s */:
            case 64941 /* i32x4_shr_u */:
            case 64942 /* i32x4_add */:
            case 64945 /* i32x4_sub */:
            case 64949 /* i32x4_mul */:
            case 64950 /* i32x4_min_s */:
            case 64951 /* i32x4_min_u */:
            case 64952 /* i32x4_max_s */:
            case 64953 /* i32x4_max_u */:
            case 64954 /* i32x4_dot_i16x8_s */:
            case 64956 /* i32x4_extmul_low_i16x8_s */:
            case 64957 /* i32x4_extmul_high_i16x8_s */:
            case 64958 /* i32x4_extmul_low_i16x8_u */:
            case 64959 /* i32x4_extmul_high_i16x8_u */:
            case 64960 /* i64x2_abs */:
            case 64961 /* i64x2_neg */:
            case 64963 /* i64x2_all_true */:
            case 64964 /* i64x2_bitmask */:
            case 64967 /* i64x2_extend_low_i32x4_s */:
            case 64968 /* i64x2_extend_high_i32x4_s */:
            case 64969 /* i64x2_extend_low_i32x4_u */:
            case 64970 /* i64x2_extend_high_i32x4_u */:
            case 64971 /* i64x2_shl */:
            case 64972 /* i64x2_shr_s */:
            case 64973 /* i64x2_shr_u */:
            case 64974 /* i64x2_add */:
            case 64977 /* i64x2_sub */:
            case 64981 /* i64x2_mul */:
            case 64982 /* i64x2_eq */:
            case 64983 /* i64x2_ne */:
            case 64984 /* i64x2_lt_s */:
            case 64985 /* i64x2_gt_s */:
            case 64986 /* i64x2_le_s */:
            case 64987 /* i64x2_ge_s */:
            case 64988 /* i64x2_extmul_low_i32x4_s */:
            case 64989 /* i64x2_extmul_high_i32x4_s */:
            case 64988 /* i64x2_extmul_low_i32x4_s */:
            case 64989 /* i64x2_extmul_high_i32x4_s */:
            case 64992 /* f32x4_abs */:
            case 64992 /* f32x4_abs */:
            case 64993 /* f32x4_neg */:
            case 64995 /* f32x4_sqrt */:
            case 64996 /* f32x4_add */:
            case 64997 /* f32x4_sub */:
            case 64998 /* f32x4_mul */:
            case 64999 /* f32x4_div */:
            case 65000 /* f32x4_min */:
            case 65001 /* f32x4_max */:
            case 65002 /* f32x4_pmin */:
            case 65003 /* f32x4_pmax */:
            case 65004 /* f64x2_abs */:
            case 65005 /* f64x2_neg */:
            case 65007 /* f64x2_sqrt */:
            case 65008 /* f64x2_add */:
            case 65009 /* f64x2_sub */:
            case 65010 /* f64x2_mul */:
            case 65011 /* f64x2_div */:
            case 65012 /* f64x2_min */:
            case 65013 /* f64x2_max */:
            case 65014 /* f64x2_pmin */:
            case 65015 /* f64x2_pmax */:
            case 65016 /* i32x4_trunc_sat_f32x4_s */:
            case 65017 /* i32x4_trunc_sat_f32x4_u */:
            case 65018 /* f32x4_convert_i32x4_s */:
            case 65019 /* f32x4_convert_i32x4_u */:
            case 65020 /* i32x4_trunc_sat_f64x2_s_zero */:
            case 65021 /* i32x4_trunc_sat_f64x2_u_zero */:
            case 65022 /* f64x2_convert_low_i32x4_s */:
            case 65023 /* f64x2_convert_low_i32x4_u */:
                break;
            default:
                this.error = new Error("Unknown operator: 0x" + code.toString(16).padStart(4, "0"));
                this.state = -1 /* ERROR */;
                return true;
        }
        this.result = {
            code: code,
            blockType: undefined,
            refType: undefined,
            brDepth: undefined,
            brTable: undefined,
            funcIndex: undefined,
            typeIndex: undefined,
            localIndex: undefined,
            globalIndex: undefined,
            fieldIndex: undefined,
            memoryAddress: memoryAddress,
            literal: literal,
            segmentIndex: undefined,
            destinationIndex: undefined,
            lines: lines,
            lineIndex: lineIndex,
        };
        return true;
    };
    BinaryReader.prototype.readCodeOperator_0xfe = function () {
        var MAX_CODE_OPERATOR_0XFE_SIZE = 11;
        var pos = this._pos;
        if (!this._eof && pos + MAX_CODE_OPERATOR_0XFE_SIZE > this._length) {
            return false;
        }
        if (!this.hasVarIntBytes()) {
            return false;
        }
        var code = this.readVarUint32() | 0xfe00;
        var memoryAddress;
        switch (code) {
            case 65024 /* atomic_notify */:
            case 65025 /* i32_atomic_wait */:
            case 65026 /* i64_atomic_wait */:
            case 65040 /* i32_atomic_load */:
            case 65041 /* i64_atomic_load */:
            case 65042 /* i32_atomic_load8_u */:
            case 65043 /* i32_atomic_load16_u */:
            case 65044 /* i64_atomic_load8_u */:
            case 65045 /* i64_atomic_load16_u */:
            case 65046 /* i64_atomic_load32_u */:
            case 65047 /* i32_atomic_store */:
            case 65048 /* i64_atomic_store */:
            case 65049 /* i32_atomic_store8 */:
            case 65050 /* i32_atomic_store16 */:
            case 65051 /* i64_atomic_store8 */:
            case 65052 /* i64_atomic_store16 */:
            case 65053 /* i64_atomic_store32 */:
            case 65054 /* i32_atomic_rmw_add */:
            case 65055 /* i64_atomic_rmw_add */:
            case 65056 /* i32_atomic_rmw8_add_u */:
            case 65057 /* i32_atomic_rmw16_add_u */:
            case 65058 /* i64_atomic_rmw8_add_u */:
            case 65059 /* i64_atomic_rmw16_add_u */:
            case 65060 /* i64_atomic_rmw32_add_u */:
            case 65061 /* i32_atomic_rmw_sub */:
            case 65062 /* i64_atomic_rmw_sub */:
            case 65063 /* i32_atomic_rmw8_sub_u */:
            case 65064 /* i32_atomic_rmw16_sub_u */:
            case 65065 /* i64_atomic_rmw8_sub_u */:
            case 65066 /* i64_atomic_rmw16_sub_u */:
            case 65067 /* i64_atomic_rmw32_sub_u */:
            case 65068 /* i32_atomic_rmw_and */:
            case 65069 /* i64_atomic_rmw_and */:
            case 65070 /* i32_atomic_rmw8_and_u */:
            case 65071 /* i32_atomic_rmw16_and_u */:
            case 65072 /* i64_atomic_rmw8_and_u */:
            case 65073 /* i64_atomic_rmw16_and_u */:
            case 65074 /* i64_atomic_rmw32_and_u */:
            case 65075 /* i32_atomic_rmw_or */:
            case 65076 /* i64_atomic_rmw_or */:
            case 65077 /* i32_atomic_rmw8_or_u */:
            case 65078 /* i32_atomic_rmw16_or_u */:
            case 65079 /* i64_atomic_rmw8_or_u */:
            case 65080 /* i64_atomic_rmw16_or_u */:
            case 65081 /* i64_atomic_rmw32_or_u */:
            case 65082 /* i32_atomic_rmw_xor */:
            case 65083 /* i64_atomic_rmw_xor */:
            case 65084 /* i32_atomic_rmw8_xor_u */:
            case 65085 /* i32_atomic_rmw16_xor_u */:
            case 65086 /* i64_atomic_rmw8_xor_u */:
            case 65087 /* i64_atomic_rmw16_xor_u */:
            case 65088 /* i64_atomic_rmw32_xor_u */:
            case 65089 /* i32_atomic_rmw_xchg */:
            case 65090 /* i64_atomic_rmw_xchg */:
            case 65091 /* i32_atomic_rmw8_xchg_u */:
            case 65092 /* i32_atomic_rmw16_xchg_u */:
            case 65093 /* i64_atomic_rmw8_xchg_u */:
            case 65094 /* i64_atomic_rmw16_xchg_u */:
            case 65095 /* i64_atomic_rmw32_xchg_u */:
            case 65096 /* i32_atomic_rmw_cmpxchg */:
            case 65097 /* i64_atomic_rmw_cmpxchg */:
            case 65098 /* i32_atomic_rmw8_cmpxchg_u */:
            case 65099 /* i32_atomic_rmw16_cmpxchg_u */:
            case 65100 /* i64_atomic_rmw8_cmpxchg_u */:
            case 65101 /* i64_atomic_rmw16_cmpxchg_u */:
            case 65102 /* i64_atomic_rmw32_cmpxchg_u */:
                memoryAddress = this.readMemoryImmediate();
                break;
            case 65027 /* atomic_fence */: {
                var consistency_model = this.readUint8();
                if (consistency_model != 0) {
                    this.error = new Error("atomic.fence consistency model must be 0");
                    this.state = -1 /* ERROR */;
                    return true;
                }
                break;
            }
            default:
                this.error = new Error("Unknown operator: 0x" + code.toString(16).padStart(4, "0"));
                this.state = -1 /* ERROR */;
                return true;
        }
        this.result = {
            code: code,
            blockType: undefined,
            refType: undefined,
            brDepth: undefined,
            brTable: undefined,
            funcIndex: undefined,
            typeIndex: undefined,
            localIndex: undefined,
            globalIndex: undefined,
            fieldIndex: undefined,
            memoryAddress: memoryAddress,
            literal: undefined,
            segmentIndex: undefined,
            destinationIndex: undefined,
            lines: undefined,
            lineIndex: undefined,
        };
        return true;
    };
    BinaryReader.prototype.readCodeOperator = function () {
        switch (this.state) {
            case 30 /* CODE_OPERATOR */:
                if (this._pos >= this._functionRange.end) {
                    this.skipFunctionBody();
                    return this.read();
                }
                break;
            case 26 /* INIT_EXPRESSION_OPERATOR */:
                if (this.result &&
                    this.result.code === 11 /* end */) {
                    this.state = 27 /* END_INIT_EXPRESSION_BODY */;
                    this.result = null;
                    return true;
                }
                break;
            case 45 /* OFFSET_EXPRESSION_OPERATOR */:
                if (this.result &&
                    this.result.code === 11 /* end */) {
                    this.state = 46 /* END_OFFSET_EXPRESSION_BODY */;
                    this.result = null;
                    return true;
                }
                break;
        }
        var code, blockType, refType, brDepth, brTable, relativeDepth, funcIndex, typeIndex, tableIndex, localIndex, globalIndex, eventIndex, memoryAddress, literal, reserved;
        if (this.state === 26 /* INIT_EXPRESSION_OPERATOR */ &&
            this._sectionId === 9 /* Element */ &&
            isExternvalElementSegmentType(this._segmentType)) {
            // We are reading a `vec(funcidx)` here, which is a dense encoding
            // for a sequence of `((ref.func y) end)` instructions.
            if (this.result &&
                this.result.code === 210 /* ref_func */) {
                code = 11 /* end */;
            }
            else {
                if (!this.hasVarIntBytes())
                    return false;
                code = 210 /* ref_func */;
                funcIndex = this.readVarUint32();
            }
        }
        else {
            var MAX_CODE_OPERATOR_SIZE = 11; // i64.const or load/store
            var pos = this._pos;
            if (!this._eof && pos + MAX_CODE_OPERATOR_SIZE > this._length) {
                return false;
            }
            code = this._data[this._pos++];
            switch (code) {
                case 2 /* block */:
                case 3 /* loop */:
                case 4 /* if */:
                case 6 /* try */:
                    blockType = this.readBlockType();
                    break;
                case 12 /* br */:
                case 13 /* br_if */:
                case 212 /* br_on_null */:
                    brDepth = this.readVarUint32() >>> 0;
                    break;
                case 14 /* br_table */:
                    var tableCount = this.readVarUint32() >>> 0;
                    if (!this.hasBytes(tableCount + 1)) {
                        // We need at least (tableCount + 1) bytes
                        this._pos = pos;
                        return false;
                    }
                    brTable = [];
                    for (var i = 0; i <= tableCount; i++) {
                        // including default
                        if (!this.hasVarIntBytes()) {
                            this._pos = pos;
                            return false;
                        }
                        brTable.push(this.readVarUint32() >>> 0);
                    }
                    break;
                case 9 /* rethrow */:
                case 24 /* delegate */:
                    relativeDepth = this.readVarUint32() >>> 0;
                    break;
                case 7 /* catch */:
                case 8 /* throw */:
                    eventIndex = this.readVarInt32();
                    break;
                case 208 /* ref_null */:
                    refType = this.readHeapType();
                    break;
                case 16 /* call */:
                case 18 /* return_call */:
                case 210 /* ref_func */:
                    funcIndex = this.readVarUint32() >>> 0;
                    break;
                case 17 /* call_indirect */:
                case 19 /* return_call_indirect */:
                    typeIndex = this.readVarUint32() >>> 0;
                    reserved = this.readVarUint1();
                    break;
                case 32 /* local_get */:
                case 33 /* local_set */:
                case 34 /* local_tee */:
                    localIndex = this.readVarUint32() >>> 0;
                    break;
                case 35 /* global_get */:
                case 36 /* global_set */:
                    globalIndex = this.readVarUint32() >>> 0;
                    break;
                case 37 /* table_get */:
                case 38 /* table_set */:
                    tableIndex = this.readVarUint32() >>> 0;
                    break;
                case 40 /* i32_load */:
                case 41 /* i64_load */:
                case 42 /* f32_load */:
                case 43 /* f64_load */:
                case 44 /* i32_load8_s */:
                case 45 /* i32_load8_u */:
                case 46 /* i32_load16_s */:
                case 47 /* i32_load16_u */:
                case 48 /* i64_load8_s */:
                case 49 /* i64_load8_u */:
                case 50 /* i64_load16_s */:
                case 51 /* i64_load16_u */:
                case 52 /* i64_load32_s */:
                case 53 /* i64_load32_u */:
                case 54 /* i32_store */:
                case 55 /* i64_store */:
                case 56 /* f32_store */:
                case 57 /* f64_store */:
                case 58 /* i32_store8 */:
                case 59 /* i32_store16 */:
                case 60 /* i64_store8 */:
                case 61 /* i64_store16 */:
                case 62 /* i64_store32 */:
                    memoryAddress = this.readMemoryImmediate();
                    break;
                case 63 /* current_memory */:
                case 64 /* grow_memory */:
                    reserved = this.readVarUint1();
                    break;
                case 65 /* i32_const */:
                    literal = this.readVarInt32();
                    break;
                case 66 /* i64_const */:
                    literal = this.readVarInt64();
                    break;
                case 67 /* f32_const */:
                    literal = new DataView(this._data.buffer, this._data.byteOffset).getFloat32(this._pos, true);
                    this._pos += 4;
                    break;
                case 68 /* f64_const */:
                    literal = new DataView(this._data.buffer, this._data.byteOffset).getFloat64(this._pos, true);
                    this._pos += 8;
                    break;
                case 251 /* prefix_0xfb */:
                    if (this.readCodeOperator_0xfb()) {
                        return true;
                    }
                    this._pos = pos;
                    return false;
                case 252 /* prefix_0xfc */:
                    if (this.readCodeOperator_0xfc()) {
                        return true;
                    }
                    this._pos = pos;
                    return false;
                case 253 /* prefix_0xfd */:
                    if (this.readCodeOperator_0xfd()) {
                        return true;
                    }
                    this._pos = pos;
                    return false;
                case 254 /* prefix_0xfe */:
                    if (this.readCodeOperator_0xfe()) {
                        return true;
                    }
                    this._pos = pos;
                    return false;
                case 0 /* unreachable */:
                case 1 /* nop */:
                case 5 /* else */:
                case 10 /* unwind */:
                case 11 /* end */:
                case 15 /* return */:
                case 25 /* catch_all */:
                case 26 /* drop */:
                case 27 /* select */:
                case 69 /* i32_eqz */:
                case 70 /* i32_eq */:
                case 71 /* i32_ne */:
                case 72 /* i32_lt_s */:
                case 73 /* i32_lt_u */:
                case 74 /* i32_gt_s */:
                case 75 /* i32_gt_u */:
                case 76 /* i32_le_s */:
                case 77 /* i32_le_u */:
                case 78 /* i32_ge_s */:
                case 79 /* i32_ge_u */:
                case 80 /* i64_eqz */:
                case 81 /* i64_eq */:
                case 82 /* i64_ne */:
                case 83 /* i64_lt_s */:
                case 84 /* i64_lt_u */:
                case 85 /* i64_gt_s */:
                case 86 /* i64_gt_u */:
                case 87 /* i64_le_s */:
                case 88 /* i64_le_u */:
                case 89 /* i64_ge_s */:
                case 90 /* i64_ge_u */:
                case 91 /* f32_eq */:
                case 92 /* f32_ne */:
                case 93 /* f32_lt */:
                case 94 /* f32_gt */:
                case 95 /* f32_le */:
                case 96 /* f32_ge */:
                case 97 /* f64_eq */:
                case 98 /* f64_ne */:
                case 99 /* f64_lt */:
                case 100 /* f64_gt */:
                case 101 /* f64_le */:
                case 102 /* f64_ge */:
                case 103 /* i32_clz */:
                case 104 /* i32_ctz */:
                case 105 /* i32_popcnt */:
                case 106 /* i32_add */:
                case 107 /* i32_sub */:
                case 108 /* i32_mul */:
                case 109 /* i32_div_s */:
                case 110 /* i32_div_u */:
                case 111 /* i32_rem_s */:
                case 112 /* i32_rem_u */:
                case 113 /* i32_and */:
                case 114 /* i32_or */:
                case 115 /* i32_xor */:
                case 116 /* i32_shl */:
                case 117 /* i32_shr_s */:
                case 118 /* i32_shr_u */:
                case 119 /* i32_rotl */:
                case 120 /* i32_rotr */:
                case 121 /* i64_clz */:
                case 122 /* i64_ctz */:
                case 123 /* i64_popcnt */:
                case 124 /* i64_add */:
                case 125 /* i64_sub */:
                case 126 /* i64_mul */:
                case 127 /* i64_div_s */:
                case 128 /* i64_div_u */:
                case 129 /* i64_rem_s */:
                case 130 /* i64_rem_u */:
                case 131 /* i64_and */:
                case 132 /* i64_or */:
                case 133 /* i64_xor */:
                case 134 /* i64_shl */:
                case 135 /* i64_shr_s */:
                case 136 /* i64_shr_u */:
                case 137 /* i64_rotl */:
                case 138 /* i64_rotr */:
                case 139 /* f32_abs */:
                case 140 /* f32_neg */:
                case 141 /* f32_ceil */:
                case 142 /* f32_floor */:
                case 143 /* f32_trunc */:
                case 144 /* f32_nearest */:
                case 145 /* f32_sqrt */:
                case 146 /* f32_add */:
                case 147 /* f32_sub */:
                case 148 /* f32_mul */:
                case 149 /* f32_div */:
                case 150 /* f32_min */:
                case 151 /* f32_max */:
                case 152 /* f32_copysign */:
                case 153 /* f64_abs */:
                case 154 /* f64_neg */:
                case 155 /* f64_ceil */:
                case 156 /* f64_floor */:
                case 157 /* f64_trunc */:
                case 158 /* f64_nearest */:
                case 159 /* f64_sqrt */:
                case 160 /* f64_add */:
                case 161 /* f64_sub */:
                case 162 /* f64_mul */:
                case 163 /* f64_div */:
                case 164 /* f64_min */:
                case 165 /* f64_max */:
                case 166 /* f64_copysign */:
                case 167 /* i32_wrap_i64 */:
                case 168 /* i32_trunc_f32_s */:
                case 169 /* i32_trunc_f32_u */:
                case 170 /* i32_trunc_f64_s */:
                case 171 /* i32_trunc_f64_u */:
                case 172 /* i64_extend_i32_s */:
                case 173 /* i64_extend_i32_u */:
                case 174 /* i64_trunc_f32_s */:
                case 175 /* i64_trunc_f32_u */:
                case 176 /* i64_trunc_f64_s */:
                case 177 /* i64_trunc_f64_u */:
                case 178 /* f32_convert_i32_s */:
                case 179 /* f32_convert_i32_u */:
                case 180 /* f32_convert_i64_s */:
                case 181 /* f32_convert_i64_u */:
                case 182 /* f32_demote_f64 */:
                case 183 /* f64_convert_i32_s */:
                case 184 /* f64_convert_i32_u */:
                case 185 /* f64_convert_i64_s */:
                case 186 /* f64_convert_i64_u */:
                case 187 /* f64_promote_f32 */:
                case 188 /* i32_reinterpret_f32 */:
                case 189 /* i64_reinterpret_f64 */:
                case 190 /* f32_reinterpret_i32 */:
                case 191 /* f64_reinterpret_i64 */:
                case 192 /* i32_extend8_s */:
                case 193 /* i32_extend16_s */:
                case 194 /* i64_extend8_s */:
                case 195 /* i64_extend16_s */:
                case 196 /* i64_extend32_s */:
                case 20 /* call_ref */:
                case 21 /* return_call_ref */:
                case 209 /* ref_is_null */:
                case 211 /* ref_as_non_null */:
                case 213 /* ref_eq */:
                    break;
                default:
                    this.error = new Error("Unknown operator: " + code);
                    this.state = -1 /* ERROR */;
                    return true;
            }
        }
        this.result = {
            code: code,
            blockType: blockType,
            refType: refType,
            brDepth: brDepth,
            brTable: brTable,
            relativeDepth: relativeDepth,
            tableIndex: tableIndex,
            funcIndex: funcIndex,
            typeIndex: typeIndex,
            localIndex: localIndex,
            globalIndex: globalIndex,
            fieldIndex: undefined,
            eventIndex: eventIndex,
            memoryAddress: memoryAddress,
            literal: literal,
            segmentIndex: undefined,
            destinationIndex: undefined,
            lines: undefined,
            lineIndex: undefined,
        };
        return true;
    };
    BinaryReader.prototype.readFunctionBody = function () {
        if (this._sectionEntriesLeft === 0) {
            this.skipSection();
            return this.read();
        }
        if (!this.hasVarIntBytes())
            return false;
        var pos = this._pos;
        var size = this.readVarUint32() >>> 0;
        var bodyEnd = this._pos + size;
        if (!this.hasVarIntBytes()) {
            this._pos = pos;
            return false;
        }
        var localCount = this.readVarUint32() >>> 0;
        var locals = [];
        for (var i = 0; i < localCount; i++) {
            if (!this.hasVarIntBytes()) {
                this._pos = pos;
                return false;
            }
            var count = this.readVarUint32() >>> 0;
            if (!this.hasVarIntBytes()) {
                this._pos = pos;
                return false;
            }
            var type = this.readType();
            locals.push({ count: count, type: type });
        }
        var bodyStart = this._pos;
        this.state = 28 /* BEGIN_FUNCTION_BODY */;
        this.result = {
            locals: locals,
        };
        this._functionRange = new DataRange(bodyStart, bodyEnd);
        this._sectionEntriesLeft--;
        return true;
    };
    BinaryReader.prototype.readSectionHeader = function () {
        if (this._pos >= this._length && this._eof) {
            this._sectionId = -1 /* Unknown */;
            this._sectionRange = null;
            this.result = null;
            this.state = 2 /* END_WASM */;
            return true;
        }
        // TODO: Handle _eof.
        if (this._pos < this._length - 4) {
            var magicNumber = this.peekInt32();
            if (magicNumber === WASM_MAGIC_NUMBER) {
                this._sectionId = -1 /* Unknown */;
                this._sectionRange = null;
                this.result = null;
                this.state = 2 /* END_WASM */;
                return true;
            }
        }
        if (!this.hasVarIntBytes())
            return false;
        var sectionStart = this._pos;
        var id = this.readVarUint7();
        if (!this.hasVarIntBytes()) {
            this._pos = sectionStart;
            return false;
        }
        var payloadLength = this.readVarUint32() >>> 0;
        var name = null;
        var payloadEnd = this._pos + payloadLength;
        if (id == 0) {
            if (!this.hasStringBytes()) {
                this._pos = sectionStart;
                return false;
            }
            name = this.readStringBytes();
        }
        this.result = { id: id, name: name };
        this._sectionId = id;
        this._sectionRange = new DataRange(this._pos, payloadEnd);
        this.state = 3 /* BEGIN_SECTION */;
        return true;
    };
    BinaryReader.prototype.readSectionRawData = function () {
        var payloadLength = this._sectionRange.end - this._sectionRange.start;
        if (!this.hasBytes(payloadLength)) {
            return false;
        }
        this.state = 7 /* SECTION_RAW_DATA */;
        this.result = this.readBytes(payloadLength);
        return true;
    };
    BinaryReader.prototype.readSectionBody = function () {
        if (this._pos >= this._sectionRange.end) {
            this.result = null;
            this.state = 4 /* END_SECTION */;
            this._sectionId = -1 /* Unknown */;
            this._sectionRange = null;
            return true;
        }
        var currentSection = this.result;
        switch (currentSection.id) {
            case 1 /* Type */:
                if (!this.hasSectionPayload())
                    return false;
                this._sectionEntriesLeft = this.readVarUint32() >>> 0;
                return this.readTypeEntry();
            case 2 /* Import */:
                if (!this.hasSectionPayload())
                    return false;
                this._sectionEntriesLeft = this.readVarUint32() >>> 0;
                return this.readImportEntry();
            case 7 /* Export */:
                if (!this.hasSectionPayload())
                    return false;
                this._sectionEntriesLeft = this.readVarUint32() >>> 0;
                return this.readExportEntry();
            case 3 /* Function */:
                if (!this.hasSectionPayload())
                    return false;
                this._sectionEntriesLeft = this.readVarUint32() >>> 0;
                return this.readFunctionEntry();
            case 4 /* Table */:
                if (!this.hasSectionPayload())
                    return false;
                this._sectionEntriesLeft = this.readVarUint32() >>> 0;
                return this.readTableEntry();
            case 5 /* Memory */:
                if (!this.hasSectionPayload())
                    return false;
                this._sectionEntriesLeft = this.readVarUint32() >>> 0;
                return this.readMemoryEntry();
            case 6 /* Global */:
                if (!this.hasVarIntBytes())
                    return false;
                this._sectionEntriesLeft = this.readVarUint32() >>> 0;
                return this.readGlobalEntry();
            case 8 /* Start */:
                if (!this.hasVarIntBytes())
                    return false;
                this.state = 22 /* START_SECTION_ENTRY */;
                this.result = { index: this.readVarUint32() };
                return true;
            case 10 /* Code */:
                if (!this.hasVarIntBytes())
                    return false;
                this._sectionEntriesLeft = this.readVarUint32() >>> 0;
                this.state = 29 /* READING_FUNCTION_HEADER */;
                return this.readFunctionBody();
            case 9 /* Element */:
                if (!this.hasVarIntBytes())
                    return false;
                this._sectionEntriesLeft = this.readVarUint32() >>> 0;
                return this.readElementEntry();
            case 11 /* Data */:
                if (!this.hasVarIntBytes())
                    return false;
                this._sectionEntriesLeft = this.readVarUint32() >>> 0;
                return this.readDataEntry();
            case 13 /* Event */:
                if (!this.hasVarIntBytes())
                    return false;
                this._sectionEntriesLeft = this.readVarUint32() >>> 0;
                return this.readEventEntry();
            case 0 /* Custom */:
                var customSectionName = exports.bytesToString(currentSection.name);
                if (customSectionName === "name") {
                    return this.readNameEntry();
                }
                if (customSectionName.indexOf("reloc.") === 0) {
                    return this.readRelocHeader();
                }
                if (customSectionName === "linking") {
                    if (!this.hasVarIntBytes())
                        return false;
                    this._sectionEntriesLeft = this.readVarUint32() >>> 0;
                    return this.readLinkingEntry();
                }
                if (customSectionName === "sourceMappingURL") {
                    return this.readSourceMappingURL();
                }
                return this.readSectionRawData();
            default:
                this.error = new Error("Unsupported section: " + this._sectionId);
                this.state = -1 /* ERROR */;
                return true;
        }
    };
    BinaryReader.prototype.read = function () {
        switch (this.state) {
            case 0 /* INITIAL */:
                if (!this.hasBytes(8))
                    return false;
                var magicNumber = this.readUint32();
                if (magicNumber != WASM_MAGIC_NUMBER) {
                    this.error = new Error("Bad magic number");
                    this.state = -1 /* ERROR */;
                    return true;
                }
                var version = this.readUint32();
                if (version != WASM_SUPPORTED_VERSION &&
                    version != WASM_SUPPORTED_EXPERIMENTAL_VERSION) {
                    this.error = new Error("Bad version number " + version);
                    this.state = -1 /* ERROR */;
                    return true;
                }
                this.result = { magicNumber: magicNumber, version: version };
                this.state = 1 /* BEGIN_WASM */;
                return true;
            case 2 /* END_WASM */:
                this.result = null;
                this.state = 1 /* BEGIN_WASM */;
                if (this.hasMoreBytes()) {
                    this.state = 0 /* INITIAL */;
                    return this.read();
                }
                return false;
            case -1 /* ERROR */:
                return true;
            case 1 /* BEGIN_WASM */:
            case 4 /* END_SECTION */:
                return this.readSectionHeader();
            case 3 /* BEGIN_SECTION */:
                return this.readSectionBody();
            case 5 /* SKIPPING_SECTION */:
                if (!this.hasSectionPayload()) {
                    return false;
                }
                this.state = 4 /* END_SECTION */;
                this._pos = this._sectionRange.end;
                this._sectionId = -1 /* Unknown */;
                this._sectionRange = null;
                this.result = null;
                return true;
            case 32 /* SKIPPING_FUNCTION_BODY */:
                this.state = 31 /* END_FUNCTION_BODY */;
                this._pos = this._functionRange.end;
                this._functionRange = null;
                this.result = null;
                return true;
            case 11 /* TYPE_SECTION_ENTRY */:
                return this.readTypeEntry();
            case 12 /* IMPORT_SECTION_ENTRY */:
                return this.readImportEntry();
            case 17 /* EXPORT_SECTION_ENTRY */:
                return this.readExportEntry();
            case 13 /* FUNCTION_SECTION_ENTRY */:
                return this.readFunctionEntry();
            case 14 /* TABLE_SECTION_ENTRY */:
                return this.readTableEntry();
            case 15 /* MEMORY_SECTION_ENTRY */:
                return this.readMemoryEntry();
            case 23 /* EVENT_SECTION_ENTRY */:
                return this.readEventEntry();
            case 16 /* GLOBAL_SECTION_ENTRY */:
            case 40 /* END_GLOBAL_SECTION_ENTRY */:
                return this.readGlobalEntry();
            case 39 /* BEGIN_GLOBAL_SECTION_ENTRY */:
                return this.readInitExpressionBody();
            case 20 /* ELEMENT_SECTION_ENTRY */:
            case 35 /* END_ELEMENT_SECTION_ENTRY */:
                return this.readElementEntry();
            case 33 /* BEGIN_ELEMENT_SECTION_ENTRY */:
                if (isActiveElementSegmentType(this._segmentType)) {
                    return this.readOffsetExpressionBody();
                }
                else {
                    // passive or declared element segment
                    return this.readElementEntryBody();
                }
            case 34 /* ELEMENT_SECTION_ENTRY_BODY */:
                if (!this.hasVarIntBytes())
                    return false;
                this._segmentEntriesLeft = this.readVarUint32();
                if (this._segmentEntriesLeft === 0) {
                    this.state = 35 /* END_ELEMENT_SECTION_ENTRY */;
                    this.result = null;
                    return true;
                }
                return this.readInitExpressionBody();
            case 18 /* DATA_SECTION_ENTRY */:
            case 38 /* END_DATA_SECTION_ENTRY */:
                return this.readDataEntry();
            case 36 /* BEGIN_DATA_SECTION_ENTRY */:
                if (isActiveDataSegmentType(this._segmentType)) {
                    return this.readOffsetExpressionBody();
                }
                else {
                    // passive data segment
                    return this.readDataEntryBody();
                }
            case 37 /* DATA_SECTION_ENTRY_BODY */:
                this.state = 38 /* END_DATA_SECTION_ENTRY */;
                this.result = null;
                return true;
            case 27 /* END_INIT_EXPRESSION_BODY */:
                switch (this._sectionId) {
                    case 6 /* Global */:
                        this.state = 40 /* END_GLOBAL_SECTION_ENTRY */;
                        return true;
                    case 9 /* Element */:
                        if (--this._segmentEntriesLeft > 0) {
                            return this.readInitExpressionBody();
                        }
                        this.state = 35 /* END_ELEMENT_SECTION_ENTRY */;
                        this.result = null;
                        return true;
                }
                this.error = new Error("Unexpected section type: " + this._sectionId);
                this.state = -1 /* ERROR */;
                return true;
            case 46 /* END_OFFSET_EXPRESSION_BODY */:
                if (this._sectionId === 11 /* Data */) {
                    return this.readDataEntryBody();
                }
                else {
                    return this.readElementEntryBody();
                }
            case 19 /* NAME_SECTION_ENTRY */:
                return this.readNameEntry();
            case 41 /* RELOC_SECTION_HEADER */:
                if (!this.hasVarIntBytes())
                    return false;
                this._sectionEntriesLeft = this.readVarUint32() >>> 0;
                return this.readRelocEntry();
            case 21 /* LINKING_SECTION_ENTRY */:
                return this.readLinkingEntry();
            case 43 /* SOURCE_MAPPING_URL */:
                this.state = 4 /* END_SECTION */;
                this.result = null;
                return true;
            case 42 /* RELOC_SECTION_ENTRY */:
                return this.readRelocEntry();
            case 29 /* READING_FUNCTION_HEADER */:
            case 31 /* END_FUNCTION_BODY */:
                return this.readFunctionBody();
            case 28 /* BEGIN_FUNCTION_BODY */:
                this.state = 30 /* CODE_OPERATOR */;
                return this.readCodeOperator();
            case 25 /* BEGIN_INIT_EXPRESSION_BODY */:
                this.state = 26 /* INIT_EXPRESSION_OPERATOR */;
                return this.readCodeOperator();
            case 44 /* BEGIN_OFFSET_EXPRESSION_BODY */:
                this.state = 45 /* OFFSET_EXPRESSION_OPERATOR */;
                return this.readCodeOperator();
            case 30 /* CODE_OPERATOR */:
            case 26 /* INIT_EXPRESSION_OPERATOR */:
            case 45 /* OFFSET_EXPRESSION_OPERATOR */:
                return this.readCodeOperator();
            case 6 /* READING_SECTION_RAW_DATA */:
                return this.readSectionRawData();
            case 22 /* START_SECTION_ENTRY */:
            case 7 /* SECTION_RAW_DATA */:
                this.state = 4 /* END_SECTION */;
                this.result = null;
                return true;
            default:
                this.error = new Error("Unsupported state: " + this.state);
                this.state = -1 /* ERROR */;
                return true;
        }
    };
    BinaryReader.prototype.skipSection = function () {
        if (this.state === -1 /* ERROR */ ||
            this.state === 0 /* INITIAL */ ||
            this.state === 4 /* END_SECTION */ ||
            this.state === 1 /* BEGIN_WASM */ ||
            this.state === 2 /* END_WASM */)
            return;
        this.state = 5 /* SKIPPING_SECTION */;
    };
    BinaryReader.prototype.skipFunctionBody = function () {
        if (this.state !== 28 /* BEGIN_FUNCTION_BODY */ &&
            this.state !== 30 /* CODE_OPERATOR */)
            return;
        this.state = 32 /* SKIPPING_FUNCTION_BODY */;
    };
    BinaryReader.prototype.skipInitExpression = function () {
        while (this.state === 26 /* INIT_EXPRESSION_OPERATOR */)
            this.readCodeOperator();
    };
    BinaryReader.prototype.fetchSectionRawData = function () {
        if (this.state !== 3 /* BEGIN_SECTION */) {
            this.error = new Error("Unsupported state: " + this.state);
            this.state = -1 /* ERROR */;
            return;
        }
        this.state = 6 /* READING_SECTION_RAW_DATA */;
    };
    return BinaryReader;
}());
exports.BinaryReader = BinaryReader;
if (typeof TextDecoder !== "undefined") {
    try {
        exports.bytesToString = (function () {
            var utf8Decoder = new TextDecoder("utf-8");
            utf8Decoder.decode(new Uint8Array([97, 208, 144]));
            return function (b) { return utf8Decoder.decode(b); };
        })();
    }
    catch (_) {
        /* ignore */
    }
}
if (!exports.bytesToString) {
    exports.bytesToString = function (b) {
        var str = String.fromCharCode.apply(null, b);
        return decodeURIComponent(escape(str));
    };
}