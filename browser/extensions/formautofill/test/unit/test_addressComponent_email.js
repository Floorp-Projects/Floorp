"use strict";

// TODO:
// https://help.xmatters.com/ondemand/trial/valid_email_format.htm what is the allow characters???
const VALID_TESTS = [
  // Is Valid Test
  // [email1, expected]
  ["john.doe@mozilla.org", true],

  ["@mozilla.org", false], // without username
  ["john.doe@", false], // without domain
  ["john.doe@-mozilla.org", false], // domain starts with '-'
  ["john.doe@mozilla.org-", false], // domain ends with '-'
  ["john.doe@mozilla.-com.au", false], // sub-domain starts with '-'
  ["john.doe@mozilla.com-.au", false], // sub-domain ends with '-'

  ["john-doe@mozilla.org", true], // dash (ok)

  // Special characters check
  ["john.!#$%&'*+-/=?^_`{|}~doe@gmail.com", true],

  ["john.doe@work@mozilla.org", false],
  ["äbc@mail.com", false],

  ["john.doe@" + "a".repeat(63) + ".org", true],
  ["john.doe@" + "a".repeat(64) + ".org", false],

  // The following are commented out since we're using a more relax email
  // validation algorithm now.
  /*
  ["-john.doe@mozilla.org", false],        // username starts with '-'
  [".john.doe@mozilla.org", false],        // username starts with '.'
  ["john.doe-@mozilla.org", true],        // username ends with '-'      ???
  ["john.doe.@mozilla.org", true],        // username ends with '.'      ???
  ["john.doe@-mozilla.org", true],        // domain starts with '.'      ???
  ["john..doe@mozilla.org", false],        // consecutive period
  ["john.-doe@mozilla.org", false],        // period + dash
  ["john-.doe@mozilla.org", false],        // dash + period
  ["john.doe@school.123", false],
  ["peter-parker@spiderman", false],

  ["a".repeat(64) + "@mydomain.com", true],      // length of username
  ["b".repeat(65) + "@mydomain.com", false],
*/
];

const COMPARE_TESTS = [
  // Same
  ["test@mozilla.org", "test@mozilla.org", SAME],
  ["john.doe@example.com", "jOhn.doE@example.com", SAME],
  ["jan@gmail.com", "JAN@gmail.com", SAME],

  // Different
  ["jan@gmail.com", "jan1@gmail.com", DIFFERENT],
  ["jan@gmail.com", "jan@gmail.com.au", DIFFERENT],
  ["john#smith@gmail.com", "johnsmith@gmail.com", DIFFERENT],
];

const TEST_FIELD_NAME = "email";

add_setup(async () => {});

add_task(async function test_isValid() {
  runIsValidTest(VALID_TESTS, TEST_FIELD_NAME, value => {
    return { email: value };
  });
});

add_task(async function test_compare() {
  runCompareTest(COMPARE_TESTS, TEST_FIELD_NAME, value => {
    return { email: value };
  });
});
