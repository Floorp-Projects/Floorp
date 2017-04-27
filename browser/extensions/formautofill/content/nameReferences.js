/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* exported nameReferences */

"use strict";

// The data below is initially copied from
// https://cs.chromium.org/chromium/src/components/autofill/core/browser/autofill_data_util.cc?rcl=b861deff77abecff11ae6a9f6946e9cc844b9817
var nameReferences = {
  NAME_PREFIXES: [
    "1lt",
    "1st",
    "2lt",
    "2nd",
    "3rd",
    "admiral",
    "capt",
    "captain",
    "col",
    "cpt",
    "dr",
    "gen",
    "general",
    "lcdr",
    "lt",
    "ltc",
    "ltg",
    "ltjg",
    "maj",
    "major",
    "mg",
    "mr",
    "mrs",
    "ms",
    "pastor",
    "prof",
    "rep",
    "reverend",
    "rev",
    "sen",
    "st",
  ],

  NAME_SUFFIXES: [
    "b.a",
    "ba",
    "d.d.s",
    "dds",
    "i",
    "ii",
    "iii",
    "iv",
    "ix",
    "jr",
    "m.a",
    "m.d",
    "ma",
    "md",
    "ms",
    "ph.d",
    "phd",
    "sr",
    "v",
    "vi",
    "vii",
    "viii",
    "x",
  ],

  FAMILY_NAME_PREFIXES: [
    "d'",
    "de",
    "del",
    "der",
    "di",
    "la",
    "le",
    "mc",
    "san",
    "st",
    "ter",
    "van",
    "von",
  ],

  // The common and non-ambiguous CJK surnames (last names) that have more than
  // one character.
  COMMON_CJK_MULTI_CHAR_SURNAMES: [
    // Korean, taken from the list of surnames:
    // https://ko.wikipedia.org/wiki/%ED%95%9C%EA%B5%AD%EC%9D%98_%EC%84%B1%EC%94%A8_%EB%AA%A9%EB%A1%9D
    "남궁",
    "사공",
    "서문",
    "선우",
    "제갈",
    "황보",
    "독고",
    "망절",

    // Chinese, taken from the top 10 Chinese 2-character surnames:
    // https://zh.wikipedia.org/wiki/%E8%A4%87%E5%A7%93#.E5.B8.B8.E8.A6.8B.E7.9A.84.E8.A4.87.E5.A7.93
    // Simplified Chinese (mostly mainland China)
    "欧阳",
    "令狐",
    "皇甫",
    "上官",
    "司徒",
    "诸葛",
    "司马",
    "宇文",
    "呼延",
    "端木",
    // Traditional Chinese (mostly Taiwan)
    "張簡",
    "歐陽",
    "諸葛",
    "申屠",
    "尉遲",
    "司馬",
    "軒轅",
    "夏侯",
  ],

  // All Korean surnames that have more than one character, even the
  // rare/ambiguous ones.
  KOREAN_MULTI_CHAR_SURNAMES: [
    "강전",
    "남궁",
    "독고",
    "동방",
    "망절",
    "사공",
    "서문",
    "선우",
    "소봉",
    "어금",
    "장곡",
    "제갈",
    "황목",
    "황보",
  ],
};
