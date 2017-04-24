/**
 * Tests the transform algorithm in profileStorage.
 */

"use strict";

Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://formautofill/ProfileStorage.jsm");

const TEST_STORE_FILE_NAME = "test-profile.json";

const COMPUTE_TESTCASES = [
  // Empty
  {
    description: "Empty profile",
    profile: {
    },
    expectedResult: {
    },
  },

  // Address
  {
    description: "\"street-address\" with single line",
    profile: {
      "street-address": "single line",
    },
    expectedResult: {
      "street-address": "single line",
      "address-line1": "single line",
    },
  },
  {
    description: "\"street-address\" with multiple lines",
    profile: {
      "street-address": "line1\nline2\nline3",
    },
    expectedResult: {
      "street-address": "line1\nline2\nline3",
      "address-line1": "line1",
      "address-line2": "line2",
      "address-line3": "line3",
    },
  },
  {
    description: "\"street-address\" with multiple lines but line2 is omitted",
    profile: {
      "street-address": "line1\n\nline3",
    },
    expectedResult: {
      "street-address": "line1\n\nline3",
      "address-line1": "line1",
      "address-line2": "",
      "address-line3": "line3",
    },
  },
  {
    description: "\"street-address\" with 4 lines",
    profile: {
      "street-address": "line1\nline2\nline3\nline4",
    },
    expectedResult: {
      "street-address": "line1\nline2\nline3\nline4",
      "address-line1": "line1",
      "address-line2": "line2",
      "address-line3": "line3",
    },
  },
];

const NORMALIZE_TESTCASES = [
  // Empty
  {
    description: "Empty profile",
    profile: {
    },
    expectedResult: {
    },
  },

  // Address
  {
    description: "Has \"address-line1~3\" and \"street-address\" is omitted",
    profile: {
      "address-line1": "line1",
      "address-line2": "line2",
      "address-line3": "line3",
    },
    expectedResult: {
      "street-address": "line1\nline2\nline3",
    },
  },
  {
    description: "Has both \"address-line1~3\" and \"street-address\"",
    profile: {
      "street-address": "street address",
      "address-line1": "line1",
      "address-line2": "line2",
      "address-line3": "line3",
    },
    expectedResult: {
      "street-address": "street address",
    },
  },
  {
    description: "Has \"address-line2~3\" and single-line \"street-address\"",
    profile: {
      "street-address": "street address",
      "address-line2": "line2",
      "address-line3": "line3",
    },
    expectedResult: {
      "street-address": "street address\nline2\nline3",
    },
  },
  {
    description: "Has \"address-line2~3\" and multiple-line \"street-address\"",
    profile: {
      "street-address": "street address\nstreet address line 2",
      "address-line2": "line2",
      "address-line3": "line3",
    },
    expectedResult: {
      "street-address": "street address\nstreet address line 2",
    },
  },
];

let do_check_profile_matches = (expectedProfile, profile) => {
  for (let key in expectedProfile) {
    do_check_eq(expectedProfile[key], profile[key] || "");
  }
};

add_task(function* test_computeFields() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;

  let profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  COMPUTE_TESTCASES.forEach(testcase => profileStorage.add(testcase.profile));
  yield profileStorage._saveImmediately();

  profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  let profiles = profileStorage.getAll();

  for (let i in profiles) {
    do_print("Verify testcase: " + COMPUTE_TESTCASES[i].description);
    do_check_profile_matches(COMPUTE_TESTCASES[i].expectedResult, profiles[i]);
  }
});

add_task(function* test_normalizeProfile() {
  let path = getTempFile(TEST_STORE_FILE_NAME).path;

  let profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  NORMALIZE_TESTCASES.forEach(testcase => profileStorage.add(testcase.profile));
  yield profileStorage._saveImmediately();

  profileStorage = new ProfileStorage(path);
  yield profileStorage.initialize();

  let profiles = profileStorage.getAll();

  for (let i in profiles) {
    do_print("Verify testcase: " + NORMALIZE_TESTCASES[i].description);
    do_check_profile_matches(NORMALIZE_TESTCASES[i].expectedResult, profiles[i]);
  }
});
