/**
 * This test only tests values in the iframe, it does not test them on the framer
 *
 * Covers the following cases:
 *  - RFP is disabled entirely
 *  - RFP is enabled entirely
 *
 *  - (A) RFP is exempted on the framer and framee and (if needed) on another cross-origin domain
 *  - (B) RFP is exempted on the framer and framee but is not on another (if needed) cross-origin domain
 *  - (C) RFP is exempted on the framer and (if needed) on another cross-origin domain, but not the framee
 *  - (D) RFP is exempted on the framer but not the framee nor another (if needed) cross-origin domain
 *  - (E) RFP is not exempted on the framer nor the framee but (if needed) is exempted on another cross-origin domain
 *  - (F) RFP is not exempted on the framer nor the framee nor another (if needed) cross-origin domain
 *  - (G) RFP is not exempted on the framer but is on the framee and (if needed) on another cross-origin domain
 *  - (H) RFP is not exempted on the framer nor another (if needed) cross-origin domain but is on the framee
 */

"use strict";

requestLongerTimeout(3);

ChromeUtils.defineESModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
});

// =============================================================================================
// =============================================================================================

async function testTimePrecision(results, expectedResults, extraData) {
  let testDesc = extraData.testDesc;
  let precision = undefined;

  if (!expectedResults.shouldRFPApply) {
    precision = extraData.RTP_Precision;
  } else {
    precision = extraData.RFP_Precision;
  }

  for (let result of results) {
    let isRounded = isTimeValueRounded(result.value, precision);

    ok(
      isRounded,
      "Test: " +
        testDesc +
        " - '" +
        "'" +
        result.name +
        "' should be rounded to nearest " +
        precision +
        " ms; saw " +
        result.value
    );
  }
}

const RFP_TIME_ATOM_MS = 16.667;
const uri = `https://${FRAMER_DOMAIN}/browser/browser/components/resistfingerprinting/test/browser/file_reduceTimePrecision_iframer.html`;

// The first three variables are defined here; and then set for test banks below.
let extraData = {};
let extraPrefs = {};
let precision = 100;
let expectedResults = {}; // In this test, we don't have explicit expected values, but rather we expect them to be rounded

// ========================================================================================================================
// Create a function that defines all the tests
function addAllTests(extraData_, extraPrefs_) {
  add_task(
    defaultsTest.bind(
      null,
      uri,
      testTimePrecision,
      expectedResults,
      extraData_,
      extraPrefs_
    )
  );

  add_task(
    simpleRFPTest.bind(
      null,
      uri,
      testTimePrecision,
      expectedResults,
      extraData_,
      extraPrefs_
    )
  );

  // (A) RFP is exempted on the framer and framee and (if needed) on another cross-origin domain
  add_task(
    testA.bind(
      null,
      uri,
      testTimePrecision,
      expectedResults,
      extraData_,
      extraPrefs_
    )
  );

  // (B) RFP is exempted on the framer and framee but is not on another (if needed) cross-origin domain
  add_task(
    testB.bind(
      null,
      uri,
      testTimePrecision,
      expectedResults,
      extraData_,
      extraPrefs_
    )
  );

  // (C) RFP is exempted on the framer and (if needed) on another cross-origin domain, but not the framee
  add_task(
    testC.bind(
      null,
      uri,
      testTimePrecision,
      expectedResults,
      extraData_,
      extraPrefs_
    )
  );

  // (D) RFP is exempted on the framer but not the framee nor another (if needed) cross-origin domain
  add_task(
    testD.bind(
      null,
      uri,
      testTimePrecision,
      expectedResults,
      extraData_,
      extraPrefs_
    )
  );

  // (E) RFP is not exempted on the framer nor the framee but (if needed) is exempted on another cross-origin domain
  add_task(
    testE.bind(
      null,
      uri,
      testTimePrecision,
      expectedResults,
      extraData_,
      extraPrefs_
    )
  );

  // (F) RFP is not exempted on the framer nor the framee nor another (if needed) cross-origin domain
  add_task(
    testF.bind(
      null,
      uri,
      testTimePrecision,
      expectedResults,
      extraData_,
      extraPrefs_
    )
  );

  // (G) RFP is not exempted on the framer but is on the framee and (if needed) on another cross-origin domain
  add_task(
    testG.bind(
      null,
      uri,
      testTimePrecision,
      expectedResults,
      extraData_,
      extraPrefs_
    )
  );

  // (H) RFP is not exempted on the framer nor another (if needed) cross-origin domain but is on the framee
  add_task(
    testH.bind(
      null,
      uri,
      testTimePrecision,
      expectedResults,
      extraData_,
      extraPrefs_
    )
  );
}

// ========================================================================================================================
// First we run through all the tests with RTP's precision set to 100 ms and 133 ms.
// Because 100ms and 133ms are >= RFP's precision of 100ms, _all_ tests results should
// be rounded.
precision = 100;
extraData = {
  RFP_Precision: precision,
  RTP_Precision: precision,
};
extraPrefs = [
  [
    "privacy.resistFingerprinting.reduceTimerPrecision.microseconds",
    precision * 1000,
  ],
];
addAllTests(extraData, extraPrefs);

precision = 133;
extraData = {
  RFP_Precision: precision,
  RTP_Precision: precision,
};
extraPrefs = [
  [
    "privacy.resistFingerprinting.reduceTimerPrecision.microseconds",
    precision * 1000,
  ],
];
addAllTests(extraData, extraPrefs);

// ========================================================================================================================
// Then we run through all the tests with the precision set to its normal value.
// This will mean that in some cases we expect RFP to apply and in some we don't.

precision = RFP_TIME_ATOM_MS;
extraData = {
  RFP_Precision: precision,
  RTP_Precision: 1,
};
extraPrefs = [];
addAllTests(extraData, extraPrefs);

// ========================================================================================================================
// Finally we run through all the tests with the precision set to an unusual value
// This will mean that in some cases we expect RFP to apply and in some we don't.

precision = RFP_TIME_ATOM_MS;
extraData = {
  RFP_Precision: RFP_TIME_ATOM_MS,
  RTP_Precision: 7,
};
extraPrefs = [
  ["privacy.resistFingerprinting.reduceTimerPrecision.microseconds", 7 * 1000],
];
addAllTests(extraData, extraPrefs);
