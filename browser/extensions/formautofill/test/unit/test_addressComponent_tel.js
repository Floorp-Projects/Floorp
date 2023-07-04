/* import-globals-from head_addressComponent.js */

"use strict";

// prettier-ignore
const VALID_TESTS = [
  // US Valid format (XXX-XXX-XXXX) and first digit is between 2-9
  ["200-234-5678", true], // First digit should between 2-9
  ["100-234-5678", true], // First digit is not between 2-9, but currently not being checked
                          // when no country code is specified
  ["555-abc-1234", true], // Non-digit characters are normalized according to ITU E.161 standard
  ["55-555-5555", false], // The national number is too short (9 digits)

  ["2-800-555-1234", false], // "2" is not US country code so we treat
                             // 2-800-555-1234 as the national number, which is too long (11 digits)

  // Phone numbers with country code
  ["1-800-555-1234", true],     // Country code without plus sign
  ["+1 200-234-5678", true],    // Country code with plus sign and  with a valid national number
  ["+1 100-234-5678", false],   // National number should be between 2-9
  ["+1 55-555-5555", false],    // National number is too short (9 digits)
  ["+1 1-800-555-1234", true],  // "+1" and "1" are both treated as coutnry code so national number
                                // is a valid number (800-555-1234)
  ["+1 2-800-555-1234", false], // The national number is too long (11 digits)
  ["+1 555-abc-1234", true],    // Non-digit characters are normalized according to ITU E.161 standard
];

const COMPARE_TESTS = [
  ["+1 520-248-6621", "+15202486621", SAME],
  ["+1 520-248-6621", "1-520-248-6621", SAME],
  ["+1 520-248-6621", "1(520)248-6621", SAME],
  ["520-248-6621", "520-248-6621", SAME], // Both phone numbers don't have coutry code
  ["520-248-6621", "+1 520-248-6621", SAME], // Compare phone number with and without country code

  ["+1 520-248-6621", "248-6621", A_CONTAINS_B],
  ["520-248-6621", "248-6621", A_CONTAINS_B],
  ["0520-248-6621", "520-248-6621", A_CONTAINS_B],
  ["48-6621", "6621", A_CONTAINS_B], // Both phone number are invalid

  ["+1 520-248-6621", "+91 520-248-6622", DIFFERENT], // different national prefix and number
  ["+1 520-248-6621", "+91 520-248-6621", DIFFERENT], // Same number, different national prefix
  ["+1 520-248-6621", "+1 520-248-6622", DIFFERENT], // Same national prefix, different number
  ["520-248-6621", "+91 520-248-6622", DIFFERENT], // Same test as above but with default region
  ["520-248-6621", "+91 520-248-6621", DIFFERENT], // Same test as above but with default region
  ["520-248-6621", "+1 520-248-6622", DIFFERENT], // Same test as above but with default region
  ["520-248-6621", "520-248-6622", DIFFERENT],

  // Normalize
  ["+1 520-248-6621", "+1 ja0-bgt-mnc1", SAME],
  ["+1 1-800-555-1234", "+1 800-555-1234", SAME],

  // TODO: Support extension
  //["+64 3 331-6005", "3 331 6005#1234", A_CONTAINS_B],
];

const TEST_FIELD_NAME = "tel";

add_setup(async () => {
  Services.prefs.setBoolPref("browser.search.region", "US");

  registerCleanupFunction(function head_cleanup() {
    Services.prefs.clearUserPref("browser.search.region");
  });
});

add_task(async function test_isValid() {
  runIsValidTest(VALID_TESTS, TEST_FIELD_NAME, value => {
    return { tel: value };
  });
});

add_task(async function test_compare() {
  runCompareTest(COMPARE_TESTS, TEST_FIELD_NAME, value => {
    return { tel: value };
  });
});
