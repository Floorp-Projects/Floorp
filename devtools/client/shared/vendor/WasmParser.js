"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
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
})(SectionCode = exports.SectionCode || (exports.SectionCode = {}));
var OperatorCode;
(function (OperatorCode) {
    OperatorCode[OperatorCode["unreachable"] = 0] = "unreachable";
    OperatorCode[OperatorCode["nop"] = 1] = "nop";
    OperatorCode[OperatorCode["block"] = 2] = "block";
    OperatorCode[OperatorCode["loop"] = 3] = "loop";
    OperatorCode[OperatorCode["if"] = 4] = "if";
    OperatorCode[OperatorCode["else"] = 5] = "else";
    OperatorCode[OperatorCode["end"] = 11] = "end";
    OperatorCode[OperatorCode["br"] = 12] = "br";
    OperatorCode[OperatorCode["br_if"] = 13] = "br_if";
    OperatorCode[OperatorCode["br_table"] = 14] = "br_table";
    OperatorCode[OperatorCode["return"] = 15] = "return";
    OperatorCode[OperatorCode["call"] = 16] = "call";
    OperatorCode[OperatorCode["call_indirect"] = 17] = "call_indirect";
    OperatorCode[OperatorCode["drop"] = 26] = "drop";
    OperatorCode[OperatorCode["select"] = 27] = "select";
    OperatorCode[OperatorCode["get_local"] = 32] = "get_local";
    OperatorCode[OperatorCode["set_local"] = 33] = "set_local";
    OperatorCode[OperatorCode["tee_local"] = 34] = "tee_local";
    OperatorCode[OperatorCode["get_global"] = 35] = "get_global";
    OperatorCode[OperatorCode["set_global"] = 36] = "set_global";
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
    OperatorCode[OperatorCode["i32_trunc_s_f32"] = 168] = "i32_trunc_s_f32";
    OperatorCode[OperatorCode["i32_trunc_u_f32"] = 169] = "i32_trunc_u_f32";
    OperatorCode[OperatorCode["i32_trunc_s_f64"] = 170] = "i32_trunc_s_f64";
    OperatorCode[OperatorCode["i32_trunc_u_f64"] = 171] = "i32_trunc_u_f64";
    OperatorCode[OperatorCode["i64_extend_s_i32"] = 172] = "i64_extend_s_i32";
    OperatorCode[OperatorCode["i64_extend_u_i32"] = 173] = "i64_extend_u_i32";
    OperatorCode[OperatorCode["i64_trunc_s_f32"] = 174] = "i64_trunc_s_f32";
    OperatorCode[OperatorCode["i64_trunc_u_f32"] = 175] = "i64_trunc_u_f32";
    OperatorCode[OperatorCode["i64_trunc_s_f64"] = 176] = "i64_trunc_s_f64";
    OperatorCode[OperatorCode["i64_trunc_u_f64"] = 177] = "i64_trunc_u_f64";
    OperatorCode[OperatorCode["f32_convert_s_i32"] = 178] = "f32_convert_s_i32";
    OperatorCode[OperatorCode["f32_convert_u_i32"] = 179] = "f32_convert_u_i32";
    OperatorCode[OperatorCode["f32_convert_s_i64"] = 180] = "f32_convert_s_i64";
    OperatorCode[OperatorCode["f32_convert_u_i64"] = 181] = "f32_convert_u_i64";
    OperatorCode[OperatorCode["f32_demote_f64"] = 182] = "f32_demote_f64";
    OperatorCode[OperatorCode["f64_convert_s_i32"] = 183] = "f64_convert_s_i32";
    OperatorCode[OperatorCode["f64_convert_u_i32"] = 184] = "f64_convert_u_i32";
    OperatorCode[OperatorCode["f64_convert_s_i64"] = 185] = "f64_convert_s_i64";
    OperatorCode[OperatorCode["f64_convert_u_i64"] = 186] = "f64_convert_u_i64";
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
    OperatorCode[OperatorCode["prefix_0xfc"] = 252] = "prefix_0xfc";
    OperatorCode[OperatorCode["prefix_0xfe"] = 254] = "prefix_0xfe";
    OperatorCode[OperatorCode["i32_trunc_s_sat_f32"] = 64512] = "i32_trunc_s_sat_f32";
    OperatorCode[OperatorCode["i32_trunc_u_sat_f32"] = 64513] = "i32_trunc_u_sat_f32";
    OperatorCode[OperatorCode["i32_trunc_s_sat_f64"] = 64514] = "i32_trunc_s_sat_f64";
    OperatorCode[OperatorCode["i32_trunc_u_sat_f64"] = 64515] = "i32_trunc_u_sat_f64";
    OperatorCode[OperatorCode["i64_trunc_s_sat_f32"] = 64516] = "i64_trunc_s_sat_f32";
    OperatorCode[OperatorCode["i64_trunc_u_sat_f32"] = 64517] = "i64_trunc_u_sat_f32";
    OperatorCode[OperatorCode["i64_trunc_s_sat_f64"] = 64518] = "i64_trunc_s_sat_f64";
    OperatorCode[OperatorCode["i64_trunc_u_sat_f64"] = 64519] = "i64_trunc_u_sat_f64";
    OperatorCode[OperatorCode["atomic_wake"] = 65024] = "atomic_wake";
    OperatorCode[OperatorCode["i32_atomic_wait"] = 65025] = "i32_atomic_wait";
    OperatorCode[OperatorCode["i64_atomic_wait"] = 65026] = "i64_atomic_wait";
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
    OperatorCode[OperatorCode["i32_atomic_rmw8_u_add"] = 65056] = "i32_atomic_rmw8_u_add";
    OperatorCode[OperatorCode["i32_atomic_rmw16_u_add"] = 65057] = "i32_atomic_rmw16_u_add";
    OperatorCode[OperatorCode["i64_atomic_rmw8_u_add"] = 65058] = "i64_atomic_rmw8_u_add";
    OperatorCode[OperatorCode["i64_atomic_rmw16_u_add"] = 65059] = "i64_atomic_rmw16_u_add";
    OperatorCode[OperatorCode["i64_atomic_rmw32_u_add"] = 65060] = "i64_atomic_rmw32_u_add";
    OperatorCode[OperatorCode["i32_atomic_rmw_sub"] = 65061] = "i32_atomic_rmw_sub";
    OperatorCode[OperatorCode["i64_atomic_rmw_sub"] = 65062] = "i64_atomic_rmw_sub";
    OperatorCode[OperatorCode["i32_atomic_rmw8_u_sub"] = 65063] = "i32_atomic_rmw8_u_sub";
    OperatorCode[OperatorCode["i32_atomic_rmw16_u_sub"] = 65064] = "i32_atomic_rmw16_u_sub";
    OperatorCode[OperatorCode["i64_atomic_rmw8_u_sub"] = 65065] = "i64_atomic_rmw8_u_sub";
    OperatorCode[OperatorCode["i64_atomic_rmw16_u_sub"] = 65066] = "i64_atomic_rmw16_u_sub";
    OperatorCode[OperatorCode["i64_atomic_rmw32_u_sub"] = 65067] = "i64_atomic_rmw32_u_sub";
    OperatorCode[OperatorCode["i32_atomic_rmw_and"] = 65068] = "i32_atomic_rmw_and";
    OperatorCode[OperatorCode["i64_atomic_rmw_and"] = 65069] = "i64_atomic_rmw_and";
    OperatorCode[OperatorCode["i32_atomic_rmw8_u_and"] = 65070] = "i32_atomic_rmw8_u_and";
    OperatorCode[OperatorCode["i32_atomic_rmw16_u_and"] = 65071] = "i32_atomic_rmw16_u_and";
    OperatorCode[OperatorCode["i64_atomic_rmw8_u_and"] = 65072] = "i64_atomic_rmw8_u_and";
    OperatorCode[OperatorCode["i64_atomic_rmw16_u_and"] = 65073] = "i64_atomic_rmw16_u_and";
    OperatorCode[OperatorCode["i64_atomic_rmw32_u_and"] = 65074] = "i64_atomic_rmw32_u_and";
    OperatorCode[OperatorCode["i32_atomic_rmw_or"] = 65075] = "i32_atomic_rmw_or";
    OperatorCode[OperatorCode["i64_atomic_rmw_or"] = 65076] = "i64_atomic_rmw_or";
    OperatorCode[OperatorCode["i32_atomic_rmw8_u_or"] = 65077] = "i32_atomic_rmw8_u_or";
    OperatorCode[OperatorCode["i32_atomic_rmw16_u_or"] = 65078] = "i32_atomic_rmw16_u_or";
    OperatorCode[OperatorCode["i64_atomic_rmw8_u_or"] = 65079] = "i64_atomic_rmw8_u_or";
    OperatorCode[OperatorCode["i64_atomic_rmw16_u_or"] = 65080] = "i64_atomic_rmw16_u_or";
    OperatorCode[OperatorCode["i64_atomic_rmw32_u_or"] = 65081] = "i64_atomic_rmw32_u_or";
    OperatorCode[OperatorCode["i32_atomic_rmw_xor"] = 65082] = "i32_atomic_rmw_xor";
    OperatorCode[OperatorCode["i64_atomic_rmw_xor"] = 65083] = "i64_atomic_rmw_xor";
    OperatorCode[OperatorCode["i32_atomic_rmw8_u_xor"] = 65084] = "i32_atomic_rmw8_u_xor";
    OperatorCode[OperatorCode["i32_atomic_rmw16_u_xor"] = 65085] = "i32_atomic_rmw16_u_xor";
    OperatorCode[OperatorCode["i64_atomic_rmw8_u_xor"] = 65086] = "i64_atomic_rmw8_u_xor";
    OperatorCode[OperatorCode["i64_atomic_rmw16_u_xor"] = 65087] = "i64_atomic_rmw16_u_xor";
    OperatorCode[OperatorCode["i64_atomic_rmw32_u_xor"] = 65088] = "i64_atomic_rmw32_u_xor";
    OperatorCode[OperatorCode["i32_atomic_rmw_xchg"] = 65089] = "i32_atomic_rmw_xchg";
    OperatorCode[OperatorCode["i64_atomic_rmw_xchg"] = 65090] = "i64_atomic_rmw_xchg";
    OperatorCode[OperatorCode["i32_atomic_rmw8_u_xchg"] = 65091] = "i32_atomic_rmw8_u_xchg";
    OperatorCode[OperatorCode["i32_atomic_rmw16_u_xchg"] = 65092] = "i32_atomic_rmw16_u_xchg";
    OperatorCode[OperatorCode["i64_atomic_rmw8_u_xchg"] = 65093] = "i64_atomic_rmw8_u_xchg";
    OperatorCode[OperatorCode["i64_atomic_rmw16_u_xchg"] = 65094] = "i64_atomic_rmw16_u_xchg";
    OperatorCode[OperatorCode["i64_atomic_rmw32_u_xchg"] = 65095] = "i64_atomic_rmw32_u_xchg";
    OperatorCode[OperatorCode["i32_atomic_rmw_cmpxchg"] = 65096] = "i32_atomic_rmw_cmpxchg";
    OperatorCode[OperatorCode["i64_atomic_rmw_cmpxchg"] = 65097] = "i64_atomic_rmw_cmpxchg";
    OperatorCode[OperatorCode["i32_atomic_rmw8_u_cmpxchg"] = 65098] = "i32_atomic_rmw8_u_cmpxchg";
    OperatorCode[OperatorCode["i32_atomic_rmw16_u_cmpxchg"] = 65099] = "i32_atomic_rmw16_u_cmpxchg";
    OperatorCode[OperatorCode["i64_atomic_rmw8_u_cmpxchg"] = 65100] = "i64_atomic_rmw8_u_cmpxchg";
    OperatorCode[OperatorCode["i64_atomic_rmw16_u_cmpxchg"] = 65101] = "i64_atomic_rmw16_u_cmpxchg";
    OperatorCode[OperatorCode["i64_atomic_rmw32_u_cmpxchg"] = 65102] = "i64_atomic_rmw32_u_cmpxchg";
})(OperatorCode = exports.OperatorCode || (exports.OperatorCode = {}));
;
exports.OperatorCodeNames = [
    "unreachable", "nop", "block", "loop", "if", "else", undefined, undefined, undefined, undefined, undefined, "end", "br", "br_if", "br_table", "return", "call", "call_indirect", undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, "drop", "select", undefined, undefined, undefined, undefined, "get_local", "set_local", "tee_local", "get_global", "set_global", undefined, undefined, undefined, "i32.load", "i64.load", "f32.load", "f64.load", "i32.load8_s", "i32.load8_u", "i32.load16_s", "i32.load16_u", "i64.load8_s", "i64.load8_u", "i64.load16_s", "i64.load16_u", "i64.load32_s", "i64.load32_u", "i32.store", "i64.store", "f32.store", "f64.store", "i32.store8", "i32.store16", "i64.store8", "i64.store16", "i64.store32", "current_memory", "grow_memory", "i32.const", "i64.const", "f32.const", "f64.const", "i32.eqz", "i32.eq", "i32.ne", "i32.lt_s", "i32.lt_u", "i32.gt_s", "i32.gt_u", "i32.le_s", "i32.le_u", "i32.ge_s", "i32.ge_u", "i64.eqz", "i64.eq", "i64.ne", "i64.lt_s", "i64.lt_u", "i64.gt_s", "i64.gt_u", "i64.le_s", "i64.le_u", "i64.ge_s", "i64.ge_u", "f32.eq", "f32.ne", "f32.lt", "f32.gt", "f32.le", "f32.ge", "f64.eq", "f64.ne", "f64.lt", "f64.gt", "f64.le", "f64.ge", "i32.clz", "i32.ctz", "i32.popcnt", "i32.add", "i32.sub", "i32.mul", "i32.div_s", "i32.div_u", "i32.rem_s", "i32.rem_u", "i32.and", "i32.or", "i32.xor", "i32.shl", "i32.shr_s", "i32.shr_u", "i32.rotl", "i32.rotr", "i64.clz", "i64.ctz", "i64.popcnt", "i64.add", "i64.sub", "i64.mul", "i64.div_s", "i64.div_u", "i64.rem_s", "i64.rem_u", "i64.and", "i64.or", "i64.xor", "i64.shl", "i64.shr_s", "i64.shr_u", "i64.rotl", "i64.rotr", "f32.abs", "f32.neg", "f32.ceil", "f32.floor", "f32.trunc", "f32.nearest", "f32.sqrt", "f32.add", "f32.sub", "f32.mul", "f32.div", "f32.min", "f32.max", "f32.copysign", "f64.abs", "f64.neg", "f64.ceil", "f64.floor", "f64.trunc", "f64.nearest", "f64.sqrt", "f64.add", "f64.sub", "f64.mul", "f64.div", "f64.min", "f64.max", "f64.copysign", "i32.wrap/i64", "i32.trunc_s/f32", "i32.trunc_u/f32", "i32.trunc_s/f64", "i32.trunc_u/f64", "i64.extend_s/i32", "i64.extend_u/i32", "i64.trunc_s/f32", "i64.trunc_u/f32", "i64.trunc_s/f64", "i64.trunc_u/f64", "f32.convert_s/i32", "f32.convert_u/i32", "f32.convert_s/i64", "f32.convert_u/i64", "f32.demote/f64", "f64.convert_s/i32", "f64.convert_u/i32", "f64.convert_s/i64", "f64.convert_u/i64", "f64.promote/f32", "i32.reinterpret/f32", "i64.reinterpret/f64", "f32.reinterpret/i32", "f64.reinterpret/i64", "i32.extend8_s", "i32.extend16_s", "i64.extend8_s", "i64.extend16_s", "i64.extend32_s", undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined
];
["i32.trunc_s:sat/f32", "i32.trunc_u:sat/f32", "i32.trunc_s:sat/f64", "i32.trunc_u:sat/f64", "i64.trunc_s:sat/f32", "i64.trunc_u:sat/f32", "i64.trunc_s:sat/f64", "i64.trunc_u:sat/f64"].forEach(function (s, i) {
    exports.OperatorCodeNames[0xfc00 | i] = s;
});
["atomic.wake", "i32.atomic.wait", "i64.atomic.wait", undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, undefined, "i32.atomic.load", "i64.atomic.load", "i32.atomic.load8_u", "i32.atomic.load16_u", "i64.atomic.load8_u", "i64.atomic.load16_u", "i64.atomic.load32_u", "i32.atomic.store", "i64.atomic.store", "i32.atomic.store8", "i32.atomic.store16", "i64.atomic.store8", "i64.atomic.store16", "i64.atomic.store32", "i32.atomic.rmw.add", "i64.atomic.rmw.add", "i32.atomic.rmw8_u.add", "i32.atomic.rmw16_u.add", "i64.atomic.rmw8_u.add", "i64.atomic.rmw16_u.add", "i64.atomic.rmw32_u.add", "i32.atomic.rmw.sub", "i64.atomic.rmw.sub", "i32.atomic.rmw8_u.sub", "i32.atomic.rmw16_u.sub", "i64.atomic.rmw8_u.sub", "i64.atomic.rmw16_u.sub", "i64.atomic.rmw32_u.sub", "i32.atomic.rmw.and", "i64.atomic.rmw.and", "i32.atomic.rmw8_u.and", "i32.atomic.rmw16_u.and", "i64.atomic.rmw8_u.and", "i64.atomic.rmw16_u.and", "i64.atomic.rmw32_u.and", "i32.atomic.rmw.or", "i64.atomic.rmw.or", "i32.atomic.rmw8_u.or", "i32.atomic.rmw16_u.or", "i64.atomic.rmw8_u.or", "i64.atomic.rmw16_u.or", "i64.atomic.rmw32_u.or", "i32.atomic.rmw.xor", "i64.atomic.rmw.xor", "i32.atomic.rmw8_u.xor", "i32.atomic.rmw16_u.xor", "i64.atomic.rmw8_u.xor", "i64.atomic.rmw16_u.xor", "i64.atomic.rmw32_u.xor", "i32.atomic.rmw.xchg", "i64.atomic.rmw.xchg", "i32.atomic.rmw8_u.xchg", "i32.atomic.rmw16_u.xchg", "i64.atomic.rmw8_u.xchg", "i64.atomic.rmw16_u.xchg", "i64.atomic.rmw32_u.xchg", "i32.atomic.rmw.cmpxchg", "i64.atomic.rmw.cmpxchg", "i32.atomic.rmw8_u.cmpxchg", "i32.atomic.rmw16_u.cmpxchg", "i64.atomic.rmw8_u.cmpxchg", "i64.atomic.rmw16_u.cmpxchg", "i64.atomic.rmw32_u.cmpxchg"].forEach(function (s, i) {
    exports.OperatorCodeNames[0xfe00 | i] = s;
});
var ExternalKind;
(function (ExternalKind) {
    ExternalKind[ExternalKind["Function"] = 0] = "Function";
    ExternalKind[ExternalKind["Table"] = 1] = "Table";
    ExternalKind[ExternalKind["Memory"] = 2] = "Memory";
    ExternalKind[ExternalKind["Global"] = 3] = "Global";
})(ExternalKind = exports.ExternalKind || (exports.ExternalKind = {}));
var Type;
(function (Type) {
    Type[Type["i32"] = -1] = "i32";
    Type[Type["i64"] = -2] = "i64";
    Type[Type["f32"] = -3] = "f32";
    Type[Type["f64"] = -4] = "f64";
    Type[Type["anyfunc"] = -16] = "anyfunc";
    Type[Type["func"] = -32] = "func";
    Type[Type["empty_block_type"] = -64] = "empty_block_type";
})(Type = exports.Type || (exports.Type = {}));
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
})(BinaryReaderState = exports.BinaryReaderState || (exports.BinaryReaderState = {}));
var DataRange = (function () {
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
var Int64 = (function () {
    function Int64(data) {
        this._data = data || new Uint8Array(8);
    }
    Int64.prototype.toInt32 = function () {
        return this._data[0] | (this._data[1] << 8) | (this._data[2] << 16) | (this._data[3] << 24);
    };
    Int64.prototype.toDouble = function () {
        var power = 1;
        var sum;
        if (this._data[7] & 0x80) {
            sum = -1;
            for (var i = 0; i < 8; i++, power *= 256)
                sum -= power * (0xFF ^ this._data[i]);
        }
        else {
            sum = 0;
            for (var i = 0; i < 8; i++, power *= 256)
                sum += power * this._data[i];
        }
        return sum;
    };
    Object.defineProperty(Int64.prototype, "data", {
        get: function () {
            return this._data;
        },
        enumerable: true,
        configurable: true
    });
    return Int64;
}());
exports.Int64 = Int64;
var BinaryReader = (function () {
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
    }
    Object.defineProperty(BinaryReader.prototype, "currentSection", {
        get: function () {
            return this.result; // TODO remove currentSection()
        },
        enumerable: true,
        configurable: true
    });
    Object.defineProperty(BinaryReader.prototype, "currentFunction", {
        get: function () {
            return this.result; // TODO remove currentFunction()
        },
        enumerable: true,
        configurable: true
    });
    Object.defineProperty(BinaryReader.prototype, "data", {
        get: function () {
            return this._data;
        },
        enumerable: true,
        configurable: true
    });
    Object.defineProperty(BinaryReader.prototype, "position", {
        get: function () {
            return this._pos;
        },
        enumerable: true,
        configurable: true
    });
    Object.defineProperty(BinaryReader.prototype, "length", {
        get: function () {
            return this._length;
        },
        enumerable: true,
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
    BinaryReader.prototype.readUint16 = function () {
        var b1 = this._data[this._pos++];
        var b2 = this._data[this._pos++];
        return b1 | (b2 << 8);
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
    BinaryReader.prototype.peekUint32 = function () {
        return this.peekInt32();
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
            result |= (byte & 0x7F) << shift;
            shift += 7;
            if ((byte & 0x80) === 0)
                break;
        }
        if (shift >= 32)
            return result;
        var ashift = (32 - shift);
        return (result << ashift) >> ashift;
    };
    BinaryReader.prototype.readVarUint32 = function () {
        var result = 0;
        var shift = 0;
        while (true) {
            var byte = this.readUint8();
            result |= (byte & 0x7F) << shift;
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
            c |= (byte & 0x7F) << shift;
            shift += 7;
            if (shift > 8) {
                result[i++] = c & 0xFF;
                c >>= 8;
                shift -= 8;
            }
            if ((byte & 0x80) === 0)
                break;
        }
        var ashift = (32 - shift);
        c = (c << ashift) >> ashift;
        while (i < 8) {
            result[i++] = c & 0xFF;
            c >>= 8;
        }
        return new Int64(result);
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
        var form = this.readVarInt7();
        var paramCount = this.readVarUint32() >>> 0;
        var paramTypes = new Int8Array(paramCount);
        for (var i = 0; i < paramCount; i++)
            paramTypes[i] = this.readVarInt7();
        var returnCount = this.readVarUint1();
        var returnTypes = new Int8Array(returnCount);
        for (var i = 0; i < returnCount; i++)
            returnTypes[i] = this.readVarInt7();
        return {
            form: form,
            params: paramTypes,
            returns: returnTypes
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
        var elementType = this.readVarInt7();
        var flags = this.readVarUint32() >>> 0;
        var limits = this.readResizableLimits(!!(flags & 0x01));
        return { elementType: elementType, limits: limits };
    };
    BinaryReader.prototype.readMemoryType = function () {
        var flags = this.readVarUint32() >>> 0;
        var shared = !!(flags & 0x02);
        return { limits: this.readResizableLimits(!!(flags & 0x01)), shared: shared };
    };
    BinaryReader.prototype.readGlobalType = function () {
        if (!this.hasVarIntBytes()) {
            return null;
        }
        var pos = this._pos;
        var contentType = this.readVarInt7();
        if (!this.hasVarIntBytes()) {
            this._pos = pos;
            return null;
        }
        var mutability = this.readVarUint1();
        return { contentType: contentType, mutability: mutability };
    };
    BinaryReader.prototype.readTypeEntry = function () {
        if (this._sectionEntriesLeft === 0) {
            this.skipSection();
            return this.read();
        }
        this.state = 11 /* TYPE_SECTION_ENTRY */;
        this.result = this.readFuncType();
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
        }
        this.result = {
            module: module,
            field: field,
            kind: kind,
            funcTypeIndex: funcTypeIndex,
            type: type
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
            type: globalType
        };
        this._sectionEntriesLeft--;
        return true;
    };
    BinaryReader.prototype.readElementEntry = function () {
        if (this._sectionEntriesLeft === 0) {
            this.skipSection();
            return this.read();
        }
        if (!this.hasVarIntBytes()) {
            this.state = 20 /* ELEMENT_SECTION_ENTRY */;
            return false;
        }
        var tableIndex = this.readVarUint32();
        this.state = 33 /* BEGIN_ELEMENT_SECTION_ENTRY */;
        this.result = { index: tableIndex };
        this._sectionEntriesLeft--;
        return true;
    };
    BinaryReader.prototype.readElementEntryBody = function () {
        if (!this.hasVarIntBytes())
            return false;
        var pos = this._pos;
        var numElemements = this.readVarUint32();
        if (!this.hasBytes(numElemements)) {
            // Shall have at least the numElemements amount of bytes.
            this._pos = pos;
            return false;
        }
        var elements = new Uint32Array(numElemements);
        for (var i = 0; i < numElemements; i++) {
            if (!this.hasVarIntBytes()) {
                this._pos = pos;
                return false;
            }
            elements[i] = this.readVarUint32();
        }
        this.state = 34 /* ELEMENT_SECTION_ENTRY_BODY */;
        this.result = { elements: elements };
        return true;
    };
    BinaryReader.prototype.readDataEntry = function () {
        if (this._sectionEntriesLeft === 0) {
            this.skipSection();
            return this.read();
        }
        if (!this.hasVarIntBytes()) {
            return false;
        }
        this.state = 36 /* BEGIN_DATA_SECTION_ENTRY */;
        this.result = {
            index: this.readVarUint32()
        };
        this._sectionEntriesLeft--;
        return true;
    };
    BinaryReader.prototype.readDataEntryBody = function () {
        if (!this.hasStringBytes()) {
            return false;
        }
        this.state = 37 /* DATA_SECTION_ENTRY_BODY */;
        this.result = {
            data: this.readStringBytes()
        };
        return true;
    };
    BinaryReader.prototype.readInitExpressionBody = function () {
        this.state = 25 /* BEGIN_INIT_EXPRESSION_BODY */;
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
                    moduleName: this.readStringBytes()
                };
                break;
            case 1 /* Function */:
                result = {
                    type: type,
                    names: this.readNameMap()
                };
                break;
            case 2 /* Local */:
                var funcsLength = this.readVarUint32();
                var funcs = [];
                for (var i = 0; i < funcsLength; i++) {
                    var funcIndex = this.readVarUint32();
                    funcs.push({
                        index: funcIndex,
                        locals: this.readNameMap()
                    });
                }
                result = {
                    type: type,
                    funcs: funcs
                };
                break;
            default:
                this.error = new Error("Bad name entry type: " + type);
                this.state = -1 /* ERROR */;
                return true;
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
            addend: addend
        };
        this._sectionEntriesLeft--;
        return true;
    };
    BinaryReader.prototype.readCodeOperator_0xfc = function () {
        var code = this._data[this._pos++] | 0xfc00;
        switch (code) {
            case 64512 /* i32_trunc_s_sat_f32 */:
            case 64513 /* i32_trunc_u_sat_f32 */:
            case 64514 /* i32_trunc_s_sat_f64 */:
            case 64515 /* i32_trunc_u_sat_f64 */:
            case 64516 /* i64_trunc_s_sat_f32 */:
            case 64517 /* i64_trunc_u_sat_f32 */:
            case 64518 /* i64_trunc_s_sat_f64 */:
            case 64519 /* i64_trunc_u_sat_f64 */:
                break;
            default:
                this.error = new Error("Unknown operator: " + code);
                this.state = -1 /* ERROR */;
                return true;
        }
        this.result = { code: code,
            blockType: undefined, brDepth: undefined, brTable: undefined,
            funcIndex: undefined, typeIndex: undefined, localIndex: undefined,
            globalIndex: undefined, memoryAddress: undefined, literal: undefined };
        return true;
    };
    BinaryReader.prototype.readCodeOperator_0xfe = function () {
        var MAX_CODE_OPERATOR_0XFE_SIZE = 11;
        var pos = this._pos;
        if (!this._eof && pos + MAX_CODE_OPERATOR_0XFE_SIZE > this._length) {
            return false;
        }
        var code = this._data[this._pos++] | 0xfe00;
        var memoryAddress;
        switch (code) {
            case 65024 /* atomic_wake */:
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
            case 65056 /* i32_atomic_rmw8_u_add */:
            case 65057 /* i32_atomic_rmw16_u_add */:
            case 65058 /* i64_atomic_rmw8_u_add */:
            case 65059 /* i64_atomic_rmw16_u_add */:
            case 65060 /* i64_atomic_rmw32_u_add */:
            case 65061 /* i32_atomic_rmw_sub */:
            case 65062 /* i64_atomic_rmw_sub */:
            case 65063 /* i32_atomic_rmw8_u_sub */:
            case 65064 /* i32_atomic_rmw16_u_sub */:
            case 65065 /* i64_atomic_rmw8_u_sub */:
            case 65066 /* i64_atomic_rmw16_u_sub */:
            case 65067 /* i64_atomic_rmw32_u_sub */:
            case 65068 /* i32_atomic_rmw_and */:
            case 65069 /* i64_atomic_rmw_and */:
            case 65070 /* i32_atomic_rmw8_u_and */:
            case 65071 /* i32_atomic_rmw16_u_and */:
            case 65072 /* i64_atomic_rmw8_u_and */:
            case 65073 /* i64_atomic_rmw16_u_and */:
            case 65074 /* i64_atomic_rmw32_u_and */:
            case 65075 /* i32_atomic_rmw_or */:
            case 65076 /* i64_atomic_rmw_or */:
            case 65077 /* i32_atomic_rmw8_u_or */:
            case 65078 /* i32_atomic_rmw16_u_or */:
            case 65079 /* i64_atomic_rmw8_u_or */:
            case 65080 /* i64_atomic_rmw16_u_or */:
            case 65081 /* i64_atomic_rmw32_u_or */:
            case 65082 /* i32_atomic_rmw_xor */:
            case 65083 /* i64_atomic_rmw_xor */:
            case 65084 /* i32_atomic_rmw8_u_xor */:
            case 65085 /* i32_atomic_rmw16_u_xor */:
            case 65086 /* i64_atomic_rmw8_u_xor */:
            case 65087 /* i64_atomic_rmw16_u_xor */:
            case 65088 /* i64_atomic_rmw32_u_xor */:
            case 65089 /* i32_atomic_rmw_xchg */:
            case 65090 /* i64_atomic_rmw_xchg */:
            case 65091 /* i32_atomic_rmw8_u_xchg */:
            case 65092 /* i32_atomic_rmw16_u_xchg */:
            case 65093 /* i64_atomic_rmw8_u_xchg */:
            case 65094 /* i64_atomic_rmw16_u_xchg */:
            case 65095 /* i64_atomic_rmw32_u_xchg */:
            case 65096 /* i32_atomic_rmw_cmpxchg */:
            case 65097 /* i64_atomic_rmw_cmpxchg */:
            case 65098 /* i32_atomic_rmw8_u_cmpxchg */:
            case 65099 /* i32_atomic_rmw16_u_cmpxchg */:
            case 65100 /* i64_atomic_rmw8_u_cmpxchg */:
            case 65101 /* i64_atomic_rmw16_u_cmpxchg */:
            case 65102 /* i64_atomic_rmw32_u_cmpxchg */:
                memoryAddress = this.readMemoryImmediate();
                break;
            default:
                this.error = new Error("Unknown operator: " + code);
                this.state = -1 /* ERROR */;
                return true;
        }
        this.result = { code: code,
            blockType: undefined, brDepth: undefined, brTable: undefined,
            funcIndex: undefined, typeIndex: undefined, localIndex: undefined,
            globalIndex: undefined, memoryAddress: memoryAddress, literal: undefined };
        return true;
    };
    BinaryReader.prototype.readCodeOperator = function () {
        if (this.state === 30 /* CODE_OPERATOR */ &&
            this._pos >= this._functionRange.end) {
            this.skipFunctionBody();
            return this.read();
        }
        else if (this.state === 26 /* INIT_EXPRESSION_OPERATOR */ &&
            this.result &&
            this.result.code === 11 /* end */) {
            this.state = 27 /* END_INIT_EXPRESSION_BODY */;
            this.result = null;
            return true;
        }
        var MAX_CODE_OPERATOR_SIZE = 11; // i64.const or load/store
        var pos = this._pos;
        if (!this._eof && pos + MAX_CODE_OPERATOR_SIZE > this._length) {
            return false;
        }
        var code = this._data[this._pos++] | 0;
        var blockType, brDepth, brTable, funcIndex, typeIndex, localIndex, globalIndex, memoryAddress, literal, reserved;
        switch (code) {
            case 2 /* block */:
            case 3 /* loop */:
            case 4 /* if */:
                blockType = this.readVarInt7();
                break;
            case 12 /* br */:
            case 13 /* br_if */:
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
                    if (!this.hasVarIntBytes()) {
                        this._pos = pos;
                        return false;
                    }
                    brTable.push(this.readVarUint32() >>> 0);
                }
                break;
            case 16 /* call */:
                funcIndex = this.readVarUint32() >>> 0;
                break;
            case 17 /* call_indirect */:
                typeIndex = this.readVarUint32() >>> 0;
                reserved = this.readVarUint1();
                break;
            case 32 /* get_local */:
            case 33 /* set_local */:
            case 34 /* tee_local */:
                localIndex = this.readVarUint32() >>> 0;
                break;
            case 35 /* get_global */:
            case 36 /* set_global */:
                globalIndex = this.readVarUint32() >>> 0;
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
            case 252 /* prefix_0xfc */:
                if (this.readCodeOperator_0xfc()) {
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
            case 11 /* end */:
            case 15 /* return */:
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
            case 168 /* i32_trunc_s_f32 */:
            case 169 /* i32_trunc_u_f32 */:
            case 170 /* i32_trunc_s_f64 */:
            case 171 /* i32_trunc_u_f64 */:
            case 172 /* i64_extend_s_i32 */:
            case 173 /* i64_extend_u_i32 */:
            case 174 /* i64_trunc_s_f32 */:
            case 175 /* i64_trunc_u_f32 */:
            case 176 /* i64_trunc_s_f64 */:
            case 177 /* i64_trunc_u_f64 */:
            case 178 /* f32_convert_s_i32 */:
            case 179 /* f32_convert_u_i32 */:
            case 180 /* f32_convert_s_i64 */:
            case 181 /* f32_convert_u_i64 */:
            case 182 /* f32_demote_f64 */:
            case 183 /* f64_convert_s_i32 */:
            case 184 /* f64_convert_u_i32 */:
            case 185 /* f64_convert_s_i64 */:
            case 186 /* f64_convert_u_i64 */:
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
                break;
            default:
                this.error = new Error("Unknown operator: " + code);
                this.state = -1 /* ERROR */;
                return true;
        }
        this.result = { code: code,
            blockType: blockType, brDepth: brDepth, brTable: brTable,
            funcIndex: funcIndex, typeIndex: typeIndex, localIndex: localIndex,
            globalIndex: globalIndex, memoryAddress: memoryAddress, literal: literal };
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
            var type = this.readVarInt7();
            locals.push({ count: count, type: type });
        }
        var bodyStart = this._pos;
        this.state = 28 /* BEGIN_FUNCTION_BODY */;
        this.result = {
            locals: locals
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
                this.state = 18 /* DATA_SECTION_ENTRY */;
                return this.readDataEntry();
            case 0 /* Custom */:
                var customSectionName = exports.bytesToString(currentSection.name);
                if (customSectionName === 'name') {
                    return this.readNameEntry();
                }
                if (customSectionName.indexOf('reloc.') === 0) {
                    return this.readRelocHeader();
                }
                if (customSectionName === 'linking') {
                    if (!this.hasVarIntBytes())
                        return false;
                    this._sectionEntriesLeft = this.readVarUint32() >>> 0;
                    return this.readLinkingEntry();
                }
                if (customSectionName === 'sourceMappingURL') {
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
                    this.error = new Error('Bad magic number');
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
            case 16 /* GLOBAL_SECTION_ENTRY */:
            case 40 /* END_GLOBAL_SECTION_ENTRY */:
                return this.readGlobalEntry();
            case 39 /* BEGIN_GLOBAL_SECTION_ENTRY */:
                return this.readInitExpressionBody();
            case 20 /* ELEMENT_SECTION_ENTRY */:
            case 35 /* END_ELEMENT_SECTION_ENTRY */:
                return this.readElementEntry();
            case 33 /* BEGIN_ELEMENT_SECTION_ENTRY */:
                return this.readInitExpressionBody();
            case 34 /* ELEMENT_SECTION_ENTRY_BODY */:
                this.state = 35 /* END_ELEMENT_SECTION_ENTRY */;
                this.result = null;
                return true;
            case 18 /* DATA_SECTION_ENTRY */:
            case 38 /* END_DATA_SECTION_ENTRY */:
                return this.readDataEntry();
            case 36 /* BEGIN_DATA_SECTION_ENTRY */:
                return this.readInitExpressionBody();
            case 37 /* DATA_SECTION_ENTRY_BODY */:
                this.state = 38 /* END_DATA_SECTION_ENTRY */;
                this.result = null;
                return true;
            case 27 /* END_INIT_EXPRESSION_BODY */:
                switch (this._sectionId) {
                    case 6 /* Global */:
                        this.state = 40 /* END_GLOBAL_SECTION_ENTRY */;
                        return true;
                    case 11 /* Data */:
                        return this.readDataEntryBody();
                    case 9 /* Element */:
                        return this.readElementEntryBody();
                }
                this.error = new Error("Unexpected section type: " + this._sectionId);
                this.state = -1 /* ERROR */;
                return true;
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
            case 30 /* CODE_OPERATOR */:
            case 26 /* INIT_EXPRESSION_OPERATOR */:
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
if (typeof TextDecoder !== 'undefined') {
    try {
        exports.bytesToString = function () {
            var utf8Decoder = new TextDecoder('utf-8');
            utf8Decoder.decode(new Uint8Array([97, 208, 144]));
            return function (b) { return utf8Decoder.decode(b); };
        }();
    }
    catch (_) { }
}
if (!exports.bytesToString) {
    exports.bytesToString = function (b) {
        var str = String.fromCharCode.apply(null, b);
        return decodeURIComponent(escape(str));
    };
}
