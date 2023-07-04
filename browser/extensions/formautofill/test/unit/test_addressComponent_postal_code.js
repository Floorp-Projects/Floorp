"use strict";

const VALID_TESTS = [
  { region: "US" },
  ["1234", false], // too short
  ["12345", true],
  ["123456", false], // too long
  ["1234A", false], // contain non-digit character
  ["12345-123", false],
  ["12345-1234", true],
  ["12345-12345", false],
  ["12345-1234A", false],
  ["12345 1234", true], // Do we want to allow this?
  ["12345_1234", false], // Do we want to allow this?

  { region: "CA" },
  ["M5T 1R5", true],
  ["M5T1R5", true], // no space between the first and second parts is allowed
  ["S4S 6X3", true],
  ["M5T", false], // Only the first part
  ["1R5", false], // Only the second part
  ["D1B 1A1", false], // invalid first character, D
  ["M5T 1R5A", false], // extra character at the end
  ["M5T 1R5-", false], // extra character at the end
  ["M5T-1R5", false], // hyphen in the wrong place
  ["MT5 1R5", false], // missing letter in the first part
  ["M5T 1R", false], // missing letter in the second part
  ["M5T 1R55", false], // extra digit at the end
  ["M5T 1R", false], // missing digit in the second part
  ["M5T 1R5Q", false], // invalid second-to-last letter, Q
];

const COMPARE_TESTS = [
  { region: "US" },
  ["12345", "12345", SAME],
  ["M5T 1R5", "m5t 1r5", SAME],
  ["12345-1234", "12345 1234", SAME],
  ["12345-1234", "12345", A_CONTAINS_B],
  ["12345-1234", "12345#1234", SAME], // B is invalid
  ["12345-1234", "1234", A_CONTAINS_B], // B is invalid
];

const TEST_FIELD_NAME = "postal-code";

add_setup(async () => {});

add_task(async function test_isValid() {
  runIsValidTest(VALID_TESTS, TEST_FIELD_NAME, value => {
    return { "postal-code": value };
  });
});

add_task(async function test_compare() {
  runCompareTest(COMPARE_TESTS, TEST_FIELD_NAME, value => {
    return { "postal-code": value };
  });
});
