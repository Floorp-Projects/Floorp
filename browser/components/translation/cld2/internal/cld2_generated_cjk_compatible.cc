// Copyright 2013 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//
// CJK compatible CLD2 scoring lookup table
//
#include "cld2tablesummary.h"

namespace CLD2 {

static const uint32 kCompatTableBuildDate = 20130128;    // yyyymmdd
static const uint32 kCompatTableSize = 1;          // Total Bucket count
static const uint32 kCompatTableKeyMask = 0xffffff00;    // Mask hash key
static const char* const kCompatTableRecognizedLangScripts =
  "zh-Hans zh-Hant ja-Hani ko-Hani vi-Hani za-Hani ";

// Empty table
static const IndirectProbBucket4 kCompatTable[kCompatTableSize] = {
  // key[4], words[4] in UTF-8
  // value[4]
  { {0x00000000,0x00000000,0x00000000,0x00000000}},	// [000]
};

// These are back-derived CTJKVZ probabilities from the table
//   kTargetCTJKVZProbs in cldutil.cc
// This is all part of using one-byte mappings for CJK but wanting to
// convert them to normal langprob values to share the scoring code.
static const uint32 kCompatTableSizeOne = 0;           // One-langprob count
static const uint32 kCompatTableIndSize = 239 * 2;     // Largest subscript
static const uint32 kCompatTableInd[kCompatTableIndSize] = {
  // [0000]
  0x00000000, 0x00000000, // [0] zh.0 zhT.0 ja.0 ko.0 vi.0 za.0
  0x00006142, 0x00000000, // [1] zh.0 zhT.0 ja.0 ko.0 vi.0 za.12
  0x00002d42, 0x00000000, // [2] zh.0 zhT.0 ja.0 ko.0 vi.12 za.0
  0x00000342, 0x00000000, // [3] zh.0 zhT.0 ja.0 ko.12 vi.0 za.0
  0x00000242, 0x00000000, // [4] zh.0 zhT.0 ja.12 ko.0 vi.0 za.0
  0x00001d42, 0x00000000, // [5] zh.0 zhT.12 ja.0 ko.0 vi.0 za.0
  0x00000542, 0x00000000, // [6] zh.12 zhT.0 ja.0 ko.0 vi.0 za.0
  0x2d00051f, 0x00000000, // [7] zh.8 zhT.0 ja.0 ko.0 vi.4 za.0
  0x0300051f, 0x00000000, // [8] zh.8 zhT.0 ja.0 ko.4 vi.0 za.0
  0x0200051f, 0x00000000, // [9] zh.8 zhT.0 ja.4 ko.0 vi.0 za.0
  0x1d00051f, 0x00000000, // [10] zh.8 zhT.4 ja.0 ko.0 vi.0 za.0
  0x031d05ea, 0x00000000, // [11] zh.8 zhT.2 ja.0 ko.2 vi.0 za.0
  0x0000611c, 0x00000000, // [12] zh.0 zhT.0 ja.0 ko.0 vi.0 za.8
  0x1d00021f, 0x00000000, // [13] zh.0 zhT.4 ja.8 ko.0 vi.0 za.0
  0x0500611f, 0x00000000, // [14] zh.4 zhT.0 ja.0 ko.0 vi.0 za.8
  0x0000021c, 0x00000000, // [15] zh.0 zhT.0 ja.8 ko.0 vi.0 za.0
  0x021d05ea, 0x00000000, // [16] zh.8 zhT.2 ja.2 ko.0 vi.0 za.0
  0x02001d1f, 0x00000000, // [17] zh.0 zhT.8 ja.4 ko.0 vi.0 za.0
  0x6100051f, 0x00000000, // [18] zh.8 zhT.0 ja.0 ko.0 vi.0 za.4
  0x02001d1d, 0x00000000, // [19] zh.0 zhT.8 ja.2 ko.0 vi.0 za.0
  0x05001d1f, 0x00000000, // [20] zh.4 zhT.8 ja.0 ko.0 vi.0 za.0
  0x03051dea, 0x00000000, // [21] zh.2 zhT.8 ja.0 ko.2 vi.0 za.0
  0x051d02ea, 0x00000000, // [22] zh.2 zhT.2 ja.8 ko.0 vi.0 za.0
  0x00001d1c, 0x00000000, // [23] zh.0 zhT.8 ja.0 ko.0 vi.0 za.0
  0x1d00021d, 0x00000000, // [24] zh.0 zhT.2 ja.8 ko.0 vi.0 za.0
  0x02051dea, 0x00000000, // [25] zh.2 zhT.8 ja.2 ko.0 vi.0 za.0
  0x0000051c, 0x00000000, // [26] zh.8 zhT.0 ja.0 ko.0 vi.0 za.0
  0x05001d1d, 0x00000000, // [27] zh.2 zhT.8 ja.0 ko.0 vi.0 za.0
  0x1d00051d, 0x00000000, // [28] zh.8 zhT.2 ja.0 ko.0 vi.0 za.0
  0x2d021ded, 0x00000000, // [29] zh.0 zhT.6 ja.2 ko.0 vi.2 za.0
  0x05002d10, 0x00000000, // [30] zh.2 zhT.0 ja.0 ko.0 vi.6 za.0
  0x05002d12, 0x00000000, // [31] zh.4 zhT.0 ja.0 ko.0 vi.6 za.0
  0x2d051dec, 0x00000000, // [32] zh.4 zhT.6 ja.0 ko.0 vi.4 za.0
  0x02051d10, 0x00002d01, // [33] zh.4 zhT.6 ja.2 ko.0 vi.2 za.0
  0x02051dec, 0x00002d01, // [34] zh.4 zhT.6 ja.4 ko.0 vi.2 za.0
  0x1d050212, 0x00000000, // [35] zh.5 zhT.4 ja.6 ko.0 vi.0 za.0
  0x2d000512, 0x00000000, // [36] zh.6 zhT.0 ja.0 ko.0 vi.4 za.0
  0x022d0510, 0x00000000, // [37] zh.6 zhT.0 ja.2 ko.0 vi.4 za.0
  0x2d0205ec, 0x00000000, // [38] zh.6 zhT.0 ja.4 ko.0 vi.4 za.0
  0x1d2d0510, 0x00000000, // [39] zh.6 zhT.2 ja.0 ko.0 vi.4 za.0
  0x022d0510, 0x00001d01, // [40] zh.6 zhT.2 ja.2 ko.0 vi.4 za.0
  0x1d020510, 0x00002d01, // [41] zh.6 zhT.2 ja.4 ko.0 vi.2 za.0
  0x2d1d0510, 0x00000000, // [42] zh.6 zhT.4 ja.0 ko.0 vi.2 za.0
  0x021d0510, 0x00002d01, // [43] zh.6 zhT.4 ja.2 ko.0 vi.2 za.0
  0x03000210, 0x00000000, // [44] zh.0 zhT.0 ja.6 ko.2 vi.0 za.0
  0x61021ded, 0x00000000, // [45] zh.0 zhT.6 ja.2 ko.0 vi.0 za.2
  0x021d61ed, 0x00000501, // [46] zh.2 zhT.2 ja.2 ko.0 vi.0 za.6
  0x05030210, 0x00001d01, // [47] zh.2 zhT.2 ja.6 ko.4 vi.0 za.0
  0x051d6110, 0x00000000, // [48] zh.2 zhT.4 ja.0 ko.0 vi.0 za.6
  0x05031d10, 0x00000000, // [49] zh.2 zhT.6 ja.0 ko.4 vi.0 za.0
  0x02031d10, 0x00000501, // [50] zh.2 zhT.6 ja.2 ko.4 vi.0 za.0
  0x03021dec, 0x00000501, // [51] zh.2 zhT.6 ja.4 ko.4 vi.0 za.0
  0x02056110, 0x00000000, // [52] zh.4 zhT.0 ja.2 ko.0 vi.0 za.6
  0x1d050210, 0x00000301, // [53] zh.4 zhT.2 ja.6 ko.2 vi.0 za.0
  0x051d61ec, 0x00000201, // [54] zh.4 zhT.4 ja.2 ko.0 vi.0 za.6
  0x02051dec, 0x00006101, // [55] zh.4 zhT.6 ja.4 ko.0 vi.0 za.2
  0x610205ed, 0x00000000, // [56] zh.6 zhT.0 ja.2 ko.0 vi.0 za.2
  0x611d05ed, 0x00000000, // [57] zh.6 zhT.2 ja.0 ko.0 vi.0 za.2
  0x02610510, 0x00001d01, // [58] zh.6 zhT.2 ja.2 ko.0 vi.0 za.4
  0x1d020510, 0x00006101, // [59] zh.6 zhT.2 ja.4 ko.0 vi.0 za.2
  0x61051dec, 0x00000201, // [60] zh.4 zhT.6 ja.2 ko.0 vi.0 za.4
  0x611d05ec, 0x00000201, // [61] zh.6 zhT.4 ja.2 ko.0 vi.0 za.4
  0x05006110, 0x00000000, // [62] zh.2 zhT.0 ja.0 ko.0 vi.0 za.6
  0x031d05ed, 0x00000000, // [63] zh.6 zhT.2 ja.0 ko.2 vi.0 za.0
  0x051d61ed, 0x00000000, // [64] zh.2 zhT.2 ja.0 ko.0 vi.0 za.6
  0x1d0205eb, 0x00000000, // [65] zh.6 zhT.2 ja.6 ko.0 vi.0 za.0
  0x021d0510, 0x00006101, // [66] zh.6 zhT.4 ja.2 ko.0 vi.0 za.2
  0x021d0510, 0x00000301, // [67] zh.6 zhT.4 ja.2 ko.2 vi.0 za.0
  0x02051dec, 0x00000301, // [68] zh.4 zhT.6 ja.4 ko.2 vi.0 za.0
  0x02610510, 0x00000000, // [69] zh.6 zhT.0 ja.2 ko.0 vi.0 za.4
  0x61020510, 0x00000000, // [70] zh.6 zhT.0 ja.4 ko.0 vi.0 za.2
  0x02000514, 0x00000000, // [71] zh.6 zhT.0 ja.6 ko.0 vi.0 za.0
  0x021d05ed, 0x00000000, // [72] zh.6 zhT.2 ja.2 ko.0 vi.0 za.0
  0x611d0510, 0x00000000, // [73] zh.6 zhT.4 ja.0 ko.0 vi.0 za.2
  0x1d020512, 0x00000000, // [74] zh.6 zhT.4 ja.5 ko.0 vi.0 za.0
  0x03001d10, 0x00000000, // [75] zh.0 zhT.6 ja.0 ko.2 vi.0 za.0
  0x03021ded, 0x00000000, // [76] zh.0 zhT.6 ja.2 ko.2 vi.0 za.0
  0x03051ded, 0x00000000, // [77] zh.2 zhT.6 ja.0 ko.2 vi.0 za.0
  0x02051ded, 0x00000301, // [78] zh.2 zhT.6 ja.2 ko.2 vi.0 za.0
  0x1d056110, 0x00000000, // [79] zh.4 zhT.2 ja.0 ko.0 vi.0 za.6
  0x611d05ec, 0x00000000, // [80] zh.6 zhT.4 ja.0 ko.0 vi.0 za.4
  0x031d0510, 0x00000000, // [81] zh.6 zhT.4 ja.0 ko.2 vi.0 za.0
  0x031d05eb, 0x00000000, // [82] zh.6 zhT.6 ja.0 ko.2 vi.0 za.0
  0x610205ec, 0x00000000, // [83] zh.6 zhT.0 ja.4 ko.0 vi.0 za.4
  0x1d610510, 0x00000000, // [84] zh.6 zhT.2 ja.0 ko.0 vi.0 za.4
  0x021d05eb, 0x00000301, // [85] zh.6 zhT.6 ja.2 ko.2 vi.0 za.0
  0x61051d10, 0x00000000, // [86] zh.4 zhT.6 ja.0 ko.0 vi.0 za.2
  0x05021deb, 0x00000000, // [87] zh.2 zhT.6 ja.6 ko.0 vi.0 za.0
  0x051d0212, 0x00000000, // [88] zh.4 zhT.5 ja.6 ko.0 vi.0 za.0
  0x03051d10, 0x00000000, // [89] zh.4 zhT.6 ja.0 ko.2 vi.0 za.0
  0x1d6105eb, 0x00000000, // [90] zh.6 zhT.2 ja.0 ko.0 vi.0 za.6
  0x03021d10, 0x00000000, // [91] zh.0 zhT.6 ja.4 ko.2 vi.0 za.0
  0x05000212, 0x00000000, // [92] zh.4 zhT.0 ja.6 ko.0 vi.0 za.0
  0x05021d10, 0x00000301, // [93] zh.2 zhT.6 ja.4 ko.2 vi.0 za.0
  0x61051dec, 0x00000000, // [94] zh.4 zhT.6 ja.0 ko.0 vi.0 za.4
  0x021d05ed, 0x00000000, // [95] zh.6 zhT.2 ja.2 ko.0 vi.0 za.0
  0x02051d10, 0x00000301, // [96] zh.4 zhT.6 ja.2 ko.2 vi.0 za.0
  0x05021d12, 0x00000000, // [97] zh.4 zhT.6 ja.5 ko.0 vi.0 za.0
  0x02000510, 0x00000000, // [98] zh.6 zhT.0 ja.2 ko.0 vi.0 za.0
  0x021d05ec, 0x00000000, // [99] zh.6 zhT.4 ja.4 ko.0 vi.0 za.0
  0x1d050210, 0x00000000, // [100] zh.4 zhT.2 ja.6 ko.0 vi.0 za.0
  0x05000210, 0x00000000, // [101] zh.2 zhT.0 ja.6 ko.0 vi.0 za.0
  0x051d61ec, 0x00000000, // [102] zh.4 zhT.4 ja.0 ko.0 vi.0 za.6
  0x051d02ec, 0x00000000, // [103] zh.4 zhT.4 ja.6 ko.0 vi.0 za.0
  0x02051d10, 0x00006101, // [104] zh.4 zhT.6 ja.2 ko.0 vi.0 za.2
  0x051d02ed, 0x00000000, // [105] zh.2 zhT.2 ja.6 ko.0 vi.0 za.0
  0x051d0210, 0x00000000, // [106] zh.2 zhT.4 ja.6 ko.0 vi.0 za.0
  0x02001d14, 0x00000000, // [107] zh.0 zhT.6 ja.6 ko.0 vi.0 za.0
  0x1d020510, 0x00000000, // [108] zh.6 zhT.2 ja.4 ko.0 vi.0 za.0
  0x1d000212, 0x00000000, // [109] zh.0 zhT.4 ja.6 ko.0 vi.0 za.0
  0x05006112, 0x00000000, // [110] zh.4 zhT.0 ja.0 ko.0 vi.0 za.6
  0x02051dec, 0x00000000, // [111] zh.4 zhT.6 ja.4 ko.0 vi.0 za.0
  0x61000514, 0x00000000, // [112] zh.6 zhT.0 ja.0 ko.0 vi.0 za.6
  0x61000510, 0x00000000, // [113] zh.6 zhT.0 ja.0 ko.0 vi.0 za.2
  0x02000512, 0x00000000, // [114] zh.6 zhT.0 ja.4 ko.0 vi.0 za.0
  0x021d0512, 0x00000000, // [115] zh.6 zhT.5 ja.4 ko.0 vi.0 za.0
  0x1d000210, 0x00000000, // [116] zh.0 zhT.2 ja.6 ko.0 vi.0 za.0
  0x0000020f, 0x00000000, // [117] zh.0 zhT.0 ja.6 ko.0 vi.0 za.0
  0x021d05eb, 0x00000000, // [118] zh.6 zhT.6 ja.2 ko.0 vi.0 za.0
  0x05021d10, 0x00000000, // [119] zh.2 zhT.6 ja.4 ko.0 vi.0 za.0
  0x021d0510, 0x00000000, // [120] zh.6 zhT.4 ja.2 ko.0 vi.0 za.0
  0x02051ded, 0x00000000, // [121] zh.2 zhT.6 ja.2 ko.0 vi.0 za.0
  0x05001d10, 0x00000000, // [122] zh.2 zhT.6 ja.0 ko.0 vi.0 za.0
  0x61000512, 0x00000000, // [123] zh.6 zhT.0 ja.0 ko.0 vi.0 za.4
  0x1d000512, 0x00000000, // [124] zh.6 zhT.4 ja.0 ko.0 vi.0 za.0
  0x1d000514, 0x00000000, // [125] zh.6 zhT.6 ja.0 ko.0 vi.0 za.0
  0x02051d12, 0x00000000, // [126] zh.5 zhT.6 ja.4 ko.0 vi.0 za.0
  0x00001d0f, 0x00000000, // [127] zh.0 zhT.6 ja.0 ko.0 vi.0 za.0
  0x1d000510, 0x00000000, // [128] zh.6 zhT.2 ja.0 ko.0 vi.0 za.0
  0x02001d10, 0x00000000, // [129] zh.0 zhT.6 ja.2 ko.0 vi.0 za.0
  0x02051d10, 0x00000000, // [130] zh.4 zhT.6 ja.2 ko.0 vi.0 za.0
  0x02001d12, 0x00000000, // [131] zh.0 zhT.6 ja.4 ko.0 vi.0 za.0
  0x05001d12, 0x00000000, // [132] zh.4 zhT.6 ja.0 ko.0 vi.0 za.0
  0x0000050f, 0x00000000, // [133] zh.6 zhT.0 ja.0 ko.0 vi.0 za.0
  0x021d0513, 0x00000000, // [134] zh.6 zhT.6 ja.5 ko.0 vi.0 za.0
  0x1d020513, 0x00000000, // [135] zh.6 zhT.5 ja.6 ko.0 vi.0 za.0
  0x05021d13, 0x00000000, // [136] zh.5 zhT.6 ja.6 ko.0 vi.0 za.0
  0x051d02af, 0x00000000, // [137] zh.5 zhT.5 ja.6 ko.0 vi.0 za.0
  0x02051daf, 0x00000000, // [138] zh.5 zhT.6 ja.5 ko.0 vi.0 za.0
  0x021d05af, 0x00000000, // [139] zh.6 zhT.5 ja.5 ko.0 vi.0 za.0
  0x021d0514, 0x00000000, // [140] zh.6 zhT.6 ja.6 ko.0 vi.0 za.0
  0x1d000513, 0x00000000, // [141] zh.6 zhT.5 ja.0 ko.0 vi.0 za.0
  0x02000513, 0x00000000, // [142] zh.6 zhT.0 ja.5 ko.0 vi.0 za.0
  0x02001d13, 0x00000000, // [143] zh.0 zhT.6 ja.5 ko.0 vi.0 za.0
  0x05001d13, 0x00000000, // [144] zh.5 zhT.6 ja.0 ko.0 vi.0 za.0
  0x05000213, 0x00000000, // [145] zh.5 zhT.0 ja.6 ko.0 vi.0 za.0
  0x1d000213, 0x00000000, // [146] zh.0 zhT.5 ja.6 ko.0 vi.0 za.0
  0x00002d06, 0x00000000, // [147] zh.0 zhT.0 ja.0 ko.0 vi.4 za.0
  0x00000306, 0x00000000, // [148] zh.0 zhT.0 ja.0 ko.4 vi.0 za.0
  0x051d2dee, 0x00000000, // [149] zh.2 zhT.2 ja.0 ko.0 vi.4 za.0
  0x021d2dee, 0x00000501, // [150] zh.2 zhT.2 ja.2 ko.0 vi.4 za.0
  0x2d051dee, 0x00000000, // [151] zh.2 zhT.4 ja.0 ko.0 vi.2 za.0
  0x02051dee, 0x00002d01, // [152] zh.2 zhT.4 ja.2 ko.0 vi.2 za.0
  0x05021d55, 0x00002d01, // [153] zh.2 zhT.4 ja.4 ko.0 vi.2 za.0
  0x022d0555, 0x00000000, // [154] zh.4 zhT.0 ja.2 ko.0 vi.4 za.0
  0x2d020555, 0x00000000, // [155] zh.4 zhT.0 ja.4 ko.0 vi.2 za.0
  0x2d1d05ee, 0x00000000, // [156] zh.4 zhT.2 ja.0 ko.0 vi.2 za.0
  0x021d05ee, 0x00002d01, // [157] zh.4 zhT.2 ja.2 ko.0 vi.2 za.0
  0x2d1d0555, 0x00000000, // [158] zh.4 zhT.4 ja.0 ko.0 vi.2 za.0
  0x021d0555, 0x00002d01, // [159] zh.4 zhT.4 ja.2 ko.0 vi.2 za.0
  0x021d0509, 0x00002d01, // [160] zh.4 zhT.4 ja.4 ko.0 vi.2 za.0
  0x1d0203ee, 0x00000000, // [161] zh.0 zhT.2 ja.2 ko.4 vi.0 za.0
  0x051d02ee, 0x00000301, // [162] zh.2 zhT.2 ja.4 ko.2 vi.0 za.0
  0x05021d55, 0x00006101, // [163] zh.2 zhT.4 ja.4 ko.0 vi.0 za.2
  0x05021d55, 0x00000301, // [164] zh.2 zhT.4 ja.4 ko.2 vi.0 za.0
  0x61020555, 0x00000000, // [165] zh.4 zhT.0 ja.4 ko.0 vi.0 za.2
  0x61020509, 0x00000000, // [166] zh.4 zhT.0 ja.4 ko.0 vi.0 za.4
  0x02030555, 0x00001d01, // [167] zh.4 zhT.2 ja.2 ko.4 vi.0 za.0
  0x031d0555, 0x00000000, // [168] zh.4 zhT.4 ja.0 ko.2 vi.0 za.0
  0x051d03ee, 0x00000000, // [169] zh.2 zhT.2 ja.0 ko.4 vi.0 za.0
  0x02051dee, 0x00000301, // [170] zh.2 zhT.4 ja.2 ko.2 vi.0 za.0
  0x021d0555, 0x00000301, // [171] zh.4 zhT.4 ja.2 ko.2 vi.0 za.0
  0x02000509, 0x00000000, // [172] zh.4 zhT.0 ja.4 ko.0 vi.0 za.0
  0x021d0509, 0x00006106, // [173] zh.4 zhT.4 ja.4 ko.0 vi.0 za.4
  0x03001d07, 0x00000000, // [174] zh.0 zhT.4 ja.0 ko.2 vi.0 za.0
  0x03021dee, 0x00000000, // [175] zh.0 zhT.4 ja.2 ko.2 vi.0 za.0
  0x610205ee, 0x00000000, // [176] zh.4 zhT.0 ja.2 ko.0 vi.0 za.2
  0x1d610555, 0x00000000, // [177] zh.4 zhT.2 ja.0 ko.0 vi.0 za.4
  0x021d61ee, 0x00000501, // [178] zh.2 zhT.2 ja.2 ko.0 vi.0 za.4
  0x03000507, 0x00000000, // [179] zh.4 zhT.0 ja.0 ko.2 vi.0 za.0
  0x021d0509, 0x00006101, // [180] zh.4 zhT.4 ja.4 ko.0 vi.0 za.2
  0x61000509, 0x00000000, // [181] zh.4 zhT.0 ja.0 ko.0 vi.0 za.4
  0x02610555, 0x00000000, // [182] zh.4 zhT.0 ja.2 ko.0 vi.0 za.4
  0x611d05ee, 0x00000000, // [183] zh.4 zhT.2 ja.0 ko.0 vi.0 za.2
  0x021d05ee, 0x00006101, // [184] zh.4 zhT.2 ja.2 ko.0 vi.0 za.2
  0x03051dee, 0x00000000, // [185] zh.2 zhT.4 ja.0 ko.2 vi.0 za.0
  0x051d61ee, 0x00000000, // [186] zh.2 zhT.2 ja.0 ko.0 vi.0 za.4
  0x05611d55, 0x00000000, // [187] zh.2 zhT.4 ja.0 ko.0 vi.0 za.4
  0x02611d55, 0x00000501, // [188] zh.2 zhT.4 ja.2 ko.0 vi.0 za.4
  0x1d020555, 0x00000000, // [189] zh.4 zhT.2 ja.4 ko.0 vi.0 za.0
  0x05000207, 0x00000000, // [190] zh.2 zhT.0 ja.4 ko.0 vi.0 za.0
  0x02000507, 0x00000000, // [191] zh.4 zhT.0 ja.2 ko.0 vi.0 za.0
  0x611d0509, 0x00000000, // [192] zh.4 zhT.4 ja.0 ko.0 vi.0 za.4
  0x611d0509, 0x00000201, // [193] zh.4 zhT.4 ja.2 ko.0 vi.0 za.4
  0x02001d09, 0x00000000, // [194] zh.0 zhT.4 ja.4 ko.0 vi.0 za.0
  0x611d0555, 0x00000000, // [195] zh.4 zhT.4 ja.0 ko.0 vi.0 za.2
  0x61051dee, 0x00000000, // [196] zh.2 zhT.4 ja.0 ko.0 vi.0 za.2
  0x051d02ee, 0x00000000, // [197] zh.2 zhT.2 ja.4 ko.0 vi.0 za.0
  0x1d000207, 0x00000000, // [198] zh.0 zhT.2 ja.4 ko.0 vi.0 za.0
  0x021d05ee, 0x00000000, // [199] zh.4 zhT.2 ja.2 ko.0 vi.0 za.0
  0x02051dee, 0x00006101, // [200] zh.2 zhT.4 ja.2 ko.0 vi.0 za.2
  0x021d0509, 0x00000000, // [201] zh.4 zhT.4 ja.4 ko.0 vi.0 za.0
  0x05021d55, 0x00000000, // [202] zh.2 zhT.4 ja.4 ko.0 vi.0 za.0
  0x00000206, 0x00000000, // [203] zh.0 zhT.0 ja.4 ko.0 vi.0 za.0
  0x02001d07, 0x00000000, // [204] zh.0 zhT.4 ja.2 ko.0 vi.0 za.0
  0x021d0555, 0x00006101, // [205] zh.4 zhT.4 ja.2 ko.0 vi.0 za.2
  0x02051dee, 0x00000000, // [206] zh.2 zhT.4 ja.2 ko.0 vi.0 za.0
  0x1d000507, 0x00000000, // [207] zh.4 zhT.2 ja.0 ko.0 vi.0 za.0
  0x1d000509, 0x00000000, // [208] zh.4 zhT.4 ja.0 ko.0 vi.0 za.0
  0x021d0555, 0x00000000, // [209] zh.4 zhT.4 ja.2 ko.0 vi.0 za.0
  0x05001d07, 0x00000000, // [210] zh.2 zhT.4 ja.0 ko.0 vi.0 za.0
  0x00001d06, 0x00000000, // [211] zh.0 zhT.4 ja.0 ko.0 vi.0 za.0
  0x00000506, 0x00000000, // [212] zh.4 zhT.0 ja.0 ko.0 vi.0 za.0
  0x2d000309, 0x00000000, // [213] zh.0 zhT.0 ja.0 ko.4 vi.4 za.0
  0x2d000209, 0x00000000, // [214] zh.0 zhT.0 ja.4 ko.0 vi.4 za.0
  0x03000209, 0x00000000, // [215] zh.0 zhT.0 ja.4 ko.4 vi.0 za.0
  0x2d001d09, 0x00000000, // [216] zh.0 zhT.4 ja.0 ko.0 vi.4 za.0
  0x03001d09, 0x00000000, // [217] zh.0 zhT.4 ja.0 ko.4 vi.0 za.0
  0x2d000509, 0x00000000, // [218] zh.4 zhT.0 ja.0 ko.0 vi.4 za.0
  0x03000509, 0x00000000, // [219] zh.4 zhT.0 ja.0 ko.4 vi.0 za.0
  0x00000501, 0x00000000, // [220] zh.2 zhT.0 ja.0 ko.0 vi.0 za.0
  0x00001d01, 0x00000000, // [221] zh.0 zhT.2 ja.0 ko.0 vi.0 za.0
  0x2d031d02, 0x00000000, // [222] zh.0 zhT.2 ja.0 ko.2 vi.2 za.0
  0x2d021d02, 0x00000000, // [223] zh.0 zhT.2 ja.2 ko.0 vi.2 za.0
  0x2d030502, 0x00000000, // [224] zh.2 zhT.0 ja.0 ko.2 vi.2 za.0
  0x2d020502, 0x00000000, // [225] zh.2 zhT.0 ja.2 ko.0 vi.2 za.0
  0x03020502, 0x00000000, // [226] zh.2 zhT.0 ja.2 ko.2 vi.0 za.0
  0x2d1d0502, 0x00000000, // [227] zh.2 zhT.2 ja.0 ko.0 vi.2 za.0
  0x021d0502, 0x00000301, // [228] zh.2 zhT.2 ja.2 ko.2 vi.0 za.0
  0x031d0502, 0x00000000, // [229] zh.2 zhT.2 ja.0 ko.2 vi.0 za.0
  0x1d000502, 0x00000000, // [230] zh.2 zhT.2 ja.0 ko.0 vi.0 za.0
  0x00000201, 0x00000000, // [231] zh.0 zhT.0 ja.2 ko.0 vi.0 za.0
  0x02001d02, 0x00000000, // [232] zh.0 zhT.2 ja.2 ko.0 vi.0 za.0
  0x021d0502, 0x00000000, // [233] zh.2 zhT.2 ja.2 ko.0 vi.0 za.0
  0x00000301, 0x00000000, // [234] zh.0 zhT.0 ja.0 ko.2 vi.0 za.0
  0x02000502, 0x00000000, // [235] zh.2 zhT.0 ja.2 ko.0 vi.0 za.0
  0x03001d02, 0x00000000, // [236] zh.0 zhT.2 ja.0 ko.2 vi.0 za.0
  0x03000202, 0x00000000, // [237] zh.0 zhT.0 ja.2 ko.2 vi.0 za.0
  0x03021d02, 0x00000000, // [238] zh.0 zhT.2 ja.2 ko.2 vi.0 za.0
};

extern const CLD2TableSummary kCjkCompat_obj = {
  kCompatTable,
  kCompatTableInd,
  kCompatTableSizeOne,
  kCompatTableSize,
  kCompatTableKeyMask,
  kCompatTableBuildDate,
  kCompatTableRecognizedLangScripts,
};

}       // End namespace CLD2

// End of generated tables


