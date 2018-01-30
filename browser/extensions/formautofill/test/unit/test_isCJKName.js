/**
 * Tests the "isCJKName" function of FormAutofillNameUtils object.
 */

"use strict";

Cu.import("resource://formautofill/FormAutofillNameUtils.jsm");

// Test cases is initially copied from
// https://cs.chromium.org/chromium/src/components/autofill/core/browser/autofill_data_util_unittest.cc
const TESTCASES = [
  {
    // Non-CJK language with only ASCII characters.
    fullName: "Homer Jay Simpson",
    expectedResult: false,
  },
  {
    // Non-CJK language with some ASCII characters.
    fullName: "Éloïse Paré",
    expectedResult: false,
  },
  {
    // Non-CJK language with no ASCII characters.
    fullName: "Σωκράτης",
    expectedResult: false,
  },
  {
    // (Simplified) Chinese name, Unihan.
    fullName: "刘翔",
    expectedResult: true,
  },
  {
    // (Simplified) Chinese name, Unihan, with an ASCII space.
    fullName: "成 龙",
    expectedResult: true,
  },
  {
    // Korean name, Hangul.
    fullName: "송지효",
    expectedResult: true,
  },
  {
    // Korean name, Hangul, with an 'IDEOGRAPHIC SPACE' (U+3000).
    fullName: "김　종국",
    expectedResult: true,
  },
  {
    // Japanese name, Unihan.
    fullName: "山田貴洋",
    expectedResult: true,
  },
  {
    // Japanese name, Katakana, with a 'KATAKANA MIDDLE DOT' (U+30FB).
    fullName: "ビル・ゲイツ",
    expectedResult: true,
  },
  {
    // Japanese name, Katakana, with a 'MIDDLE DOT' (U+00B7) (likely a typo).
    fullName: "ビル·ゲイツ",
    expectedResult: true,
  },
  {
    // CJK names don't have a middle name, so a 3-part name is bogus to us.
    fullName: "반 기 문",
    expectedResult: false,
  },
];

add_task(async function test_isCJKName() {
  TESTCASES.forEach(testcase => {
    info("Starting testcase: " + testcase.fullName);
    let result = FormAutofillNameUtils._isCJKName(testcase.fullName);
    Assert.equal(result, testcase.expectedResult);
  });
});
