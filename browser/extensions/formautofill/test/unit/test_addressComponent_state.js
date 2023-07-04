"use strict";

const VALID_TESTS = [
  ["California", true],
  ["california", true],
  ["Californib", false],
  ["CA", true],
  ["CA.", true],
  ["CC", false],
];

const COMPARE_TESTS = [
  ["California", "california", SAME], // case insensitive
  ["CA", "california", SAME],
  ["CA", "ca", SAME],
  ["California", "New Jersey", DIFFERENT],
  ["New York", "New Jersey", DIFFERENT],
];

const TEST_FIELD_NAME = "address-level1";

add_task(async function test_isValid() {
  runIsValidTest(VALID_TESTS, TEST_FIELD_NAME, value => {
    return { "address-level1": value };
  });
});

add_task(async function test_compare() {
  runCompareTest(COMPARE_TESTS, TEST_FIELD_NAME, value => {
    return { "address-level1": value };
  });
});
