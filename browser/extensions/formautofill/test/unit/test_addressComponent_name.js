"use strict";

const VALID_TESTS = [
  ["John Doe", true],
  ["John O'Brian'", true],
  ["John O-Brian'", true],
  ["John Doe", true],
];

// prettier-ignore
const COMPARE_TESTS = [
  // Same
  ["John", "John", SAME],                             // first name
  ["John Doe", "John Doe", SAME],                     // first and last name
  ["John Middle Doe", "John Middle Doe", SAME],       // first, middle, and last name
  ["John Mid1 Mid2 Doe", "John Mid1 Mid2 Doe", SAME],

  // Same: case insenstive
  ["John Doe", "john doe", SAME],

  // Similar: whitespaces are merged
  ["John Doe", "John  Doe", SIMILAR],

  // Similar: asscent and base
  ["John Doe", "John Döe", SIMILAR], // asscent and base

  // A Contains B
  ["John Doe", "Doe", A_CONTAINS_B],                 // first + family name contains family name
  ["John Doe", "John", A_CONTAINS_B],                // first + family name contains first name
  ["John Middle Doe", "Doe", A_CONTAINS_B],          // [first, middle, last] contains [last]
  ["John Middle Doe", "John", A_CONTAINS_B],         // [first, middle, last] contains [first]
  ["John Middle Doe", "Middle", A_CONTAINS_B],       // [first, middle, last] contains [middle]
  ["John Middle Doe", "Middle Doe", A_CONTAINS_B],   // [first, middle, last] contains [middle, last]
  ["John Middle Doe", "John Middle", A_CONTAINS_B],  // [first, middle, last] contains [fisrt, middle]
  ["John Middle Doe", "John Doe", A_CONTAINS_B],        // [first, middle, last] contains [fisrt, last]
  ["John Mary Jane Doe", "John Doe", A_CONTAINS_B],  // [first, middle, last] contains [fisrt, last]

  // Different
  ["John Doe", "Jane Roe", DIFFERENT],
  ["John Doe", "Doe John", DIFFERENT],                // swap order
  ["John Middle Doe", "Middle John", DIFFERENT],
  ["John Middle Doe", "Doe Middle", DIFFERENT],
  ["John Doe", "John Roe.", DIFFERENT],               // different family name
  ["John Doe", "Jane Doe", DIFFERENT],                // different given name
  ["John Middle Doe", "Jane Michael Doe", DIFFERENT], // different middle name

  // Puncuation is either removed or replaced with white space
  ["John O'Brian", "John OBrian", SIMILAR],
  ["John O'Brian", "John O-Brian", SIMILAR],
  ["John O'Brian", "John O Brian", SIMILAR],
  ["John-Mary Doe", "JohnMary Doe", SIMILAR],
  ["John-Mary Doe", "John'Mary Doe", SIMILAR],
  ["John-Mary Doe", "John Mary Doe", SIMILAR],
  ["John-Mary Doe", "John Mary", A_CONTAINS_B],

  // Test Name Variants
  ["John Doe", "J. Doe", A_CONTAINS_B],               // first name to initial
  ["John Doe", "J. doe", A_CONTAINS_B],
  ["John Doe", "J. Doe", A_CONTAINS_B],               // first name to initial without '.'

  ["John Middle Doe", "J. Middle Doe", A_CONTAINS_B], // first name to initial, middle name unchanged
  ["John Middle Doe", "J. Doe", A_CONTAINS_B],        // first name to initial, no middle name

  ["John Middle Doe", "John M. Doe", A_CONTAINS_B],   // middle name to initial, first name unchanged
  ["John Middle Doe", "J. M. Doe", A_CONTAINS_B],     // first and middle name to initial
  ["John Middle Doe", "J M Doe", A_CONTAINS_B],       // first and middle name to initial without '.'
  ["John Middle Doe", "John M. Doe", A_CONTAINS_B],   // middle name with initial

  // Test Name Variants: multiple middle name
  ["John Mary Jane Doe", "J. MARY JANE Doe", A_CONTAINS_B],   // first to initial
  ["John Mary Jane Doe", "john. M. J. doe", A_CONTAINS_B],    // middle name to initial
  ["John Mary Jane Doe", "J. M. J. Doe", A_CONTAINS_B],       // first & middle name to initial
  ["John Mary Jane Doe", "J. M. Doe", A_CONTAINS_B],          // first & part of the middle name to initial
  ["John Mary Jane Doe", "John M. Doe", A_CONTAINS_B],
  ["John Mary Jane Doe", "J. Doe", A_CONTAINS_B],

  // Test Name Variants: merge initials
  ["John Middle Doe", "JM Doe", A_CONTAINS_B],
  ["John Mary Jane Doe", "JMJ. doe", A_CONTAINS_B],

  // Different: Don't consider the cases when family name is abbreviated
  ["John Middle Doe", "JMD", DIFFERENT],
  ["John Middle Doe", "John Middle D.", DIFFERENT],
  ["John Middle Doe", "J. M. D.", DIFFERENT],
];

const TEST_FIELD_NAME = "name";

add_setup(async () => {});

add_task(async function test_isValid() {
  runIsValidTest(VALID_TESTS, TEST_FIELD_NAME, value => {
    return { name: value };
  });
});

add_task(async function test_compare() {
  runCompareTest(COMPARE_TESTS, TEST_FIELD_NAME, value => {
    return { name: value };
  });
});
