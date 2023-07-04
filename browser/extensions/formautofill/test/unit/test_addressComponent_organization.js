"use strict";

// prettier-ignore
const VALID_TESTS = [
  ["Mozilla", true],
  ["mozilla", true],
  ["@Mozilla", true],
  ["-!@#%&*_(){}[:;\"',.?]", false], // Not valid when the organization name only contains punctuations
];

const COMPARE_TESTS = [
  // Same
  ["Mozilla", "Mozilla", SAME], // Exact the same

  // Similar
  ["Mozilla", "mozilla", SIMILAR], // Ignore case
  ["Casavant Frères", "Casavant Freres", SIMILAR], // asscent and base
  ["Graphik Dimensions, Ltd.", "Graphik Dimensions Ltd", SIMILAR], // Punctuation is stripped and trim space in the end
  ["T & T Supermarket", "T&T Supermarket", SIMILAR], // & is stripped and merged consecutive whitespace
  ["Food & Pharmacy", "Pharmacy & Food", SIMILAR], // Same tokens, different order
  ["Johnson & Johnson", "Johnson", SIMILAR], // Can always find the same token in the other

  // A Contains B
  ["Mozilla Inc.", "Mozilla", A_CONTAINS_B], // Contain, the same prefix
  ["The Walt Disney", "Walt Disney", A_CONTAINS_B], // Contain, the same suffix
  ["Coca-Cola Company", "Coca Cola", A_CONTAINS_B], // Contain, strip punctuation

  // Different
  ["Meta", "facebook", DIFFERENT], // Completely different
  ["Metro Inc.", "CGI Inc.", DIFFERENT], // Different prefix
  ["AT&T Corp.", "AT&T Inc.", DIFFERENT], // Different suffix
  ["AT&T Corp.", "AT&T Corporation", DIFFERENT], // Different suffix
  ["Ben & Jerry's", "Ben & Jerrys", DIFFERENT], // Different because Jerry's becomes ["Jerry", "s"]
  ["Arc'teryx", "Arcteryx", DIFFERENT], // Different because Arc'teryx' becomes ["Arc", "teryx"]
  ["BMW", "Bayerische Motoren Werke", DIFFERENT],

  ["Linens 'n Things", "Linens'n Things", SIMILAR], // Punctuation is replaced with whitespace, so both strings become "Linesns n Things"
];

const TEST_FIELD_NAME = "organization";

add_setup(async () => {});

add_task(async function test_isValid() {
  runIsValidTest(VALID_TESTS, TEST_FIELD_NAME, value => {
    return { organization: value };
  });
});

add_task(async function test_compare() {
  runCompareTest(COMPARE_TESTS, TEST_FIELD_NAME, value => {
    return { organization: value };
  });
});
