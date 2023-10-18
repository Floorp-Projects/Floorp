"use strict";

const VALID_TESTS = [
  ["123 Main St. Apt 4, Floor 2", true],
  ["This is a street", true],
  ["A", true],
  ["住址", true],
  ["!#%&'*+", false],
  ["1234", false],
];

const COMPARE_TESTS = [
  { region: "US" },
  ["123 Main St.", "123 Main St.", SAME], // Exactly the same with only street number and street name
  ["123 Main St. Apt 4, Floor 2", "123 Main St. Apt 4, Floor 2", SAME],
  ["123 Main St. Apt 4A, Floor 2", "123 main St. Apt 4a, Floor 2", SAME],
  ["123 Main St. Apt 4, Floor 2", "123 Main St. Suite 4, 2nd fl", SAME], // Exactly the same after parsing
  ["Main St.", "Main St.", SAME], // Exactly the same with only street name
  ["Main St.", "main st.", SAME], // Exactly the same with only street name (case-insenstive)

  ["123 Main St.", "Main St.", A_CONTAINS_B], // Street number is mergeable
  ["123 Main Lane St.", "123 Main St.", A_CONTAINS_B], // Street name is mergeable
  ["123 Main St. Apt 4", "Main St.", A_CONTAINS_B], // Apartment number is mergeable
  ["123 Main St. Apt 4, Floor 2", "123 Main St., Floor 2", A_CONTAINS_B],
  ["123 Main St. Floor 2", "Main St.", A_CONTAINS_B], // Floor number is mergeable
  ["123 Main St. Apt 4, Floor 2", "123 Main St. Apt 4", A_CONTAINS_B],
  ["123 North-South Road", "123 North South Road", SIMILAR], // Street number is mergeable

  ["123 Main St. Apt 4, Floor 2", "1234 Main St. Apt 4, Floor 2", DIFFERENT], // Street number is different
  ["123 Main St. Apt 4, Floor 2", "123 Mainn St. Apt 4, Floor 2", DIFFERENT], // Street name is different
  [
    "123 Lane Main St. Apt 4, Floor 2",
    "123 Main Lane St. Apt 4, Floor 2",
    DIFFERENT,
  ], // Street name is different (token not in order)
  ["123 Main St. Apt 4, Floor 2", "123 Main St. Apt 41, Floor 2", DIFFERENT], // Apartment number is different
  ["123 Main St. Apt 4, Floor 2", "123 Main St. Apt 4, Floor 22", DIFFERENT], // Floor number is different

  ["123 Main St. Apt 4, Floor 2", "123 Main St. Floor 2, Apt 4", DIFFERENT],

  // When address cannot be parsed
  ["Any Address", "any address", SAME],
  ["Any Address 4F", "Any Address", A_CONTAINS_B],
  ["An Any Address 4F", "Any Address", A_CONTAINS_B],
  ["Any Address", "Address Any", DIFFERENT],
  ["Any Address", "Other Address", DIFFERENT],
];

const TEST_FIELD_NAME = "street-address";

add_setup(async () => {});

add_task(async function test_isValid() {
  runIsValidTest(VALID_TESTS, TEST_FIELD_NAME, value => {
    return { "street-address": value };
  });
});

add_task(async function test_compare() {
  runCompareTest(COMPARE_TESTS, TEST_FIELD_NAME, value => {
    return { "street-address": value };
  });
});
