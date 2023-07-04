"use strict";

const VALID_TESTS = [
  ["United States", true],
  ["Not United States", true], // Invalid country name will be replaced with the default region, so
  // it is still valid
];

const COMPARE_TESTS = [
  // United Stats, US, USA, America, U.S.A.
  { region: "US" },
  ["United States", "United States", SAME],
  ["United States", "united states", SAME],
  ["United States", "US", SAME],
  ["America", "United States", SAME],
  ["America", "US", SAME],
  ["US", "USA", SAME],
  ["United States", "U.S.A.", SAME], // Normalize

  ["USB", "US", SAME],

  // Canada, Can, CA
  ["CA", "Canada", SAME],
  ["CA", "CAN", SAME],
  ["CA", "US", DIFFERENT],

  { region: "DE" },
  ["USB", "US", DIFFERENT],
  ["United States", "Germany", DIFFERENT],

  ["Invalid Country Name", "Germany", SAME],
  ["AAA", "BBB", SAME],
];

const TEST_FIELD_NAME = "country";

add_task(async function test_isValid() {
  runIsValidTest(VALID_TESTS, TEST_FIELD_NAME, value => {
    return { country: value };
  });
});

add_task(async function test_compare() {
  runCompareTest(COMPARE_TESTS, TEST_FIELD_NAME, value => {
    return { country: value };
  });
});
