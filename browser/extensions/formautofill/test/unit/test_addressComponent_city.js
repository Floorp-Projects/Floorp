"use strict";

const VALID_TESTS = [["New York City", true]];

const COMPARE_TESTS = [
  ["New York City", "New York City", SAME],
  ["New York City", "new york city", SAME],
  ["New York City", "New York  City", SIMILAR], // Merge whitespace
  ["Happy Valley-Goose Bay", "Happy Valley Goose Bay", SIMILAR], // Replace punctuation with whitespace
  ["New York City", "New York", A_CONTAINS_B],
  ["New York", "NewYork", DIFFERENT],
  ["New York City", "City New York", DIFFERENT],
];

const TEST_FIELD_NAME = "address-level2";

add_task(async function test_isValid() {
  runIsValidTest(VALID_TESTS, TEST_FIELD_NAME, value => {
    return { "address-level2": value };
  });
});

add_task(async function test_compare() {
  runCompareTest(COMPARE_TESTS, TEST_FIELD_NAME, value => {
    return { "address-level2": value };
  });
});
