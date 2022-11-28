/**
 * This test only tests values in the iframe, it does not test them on the framer
 *
 * Covers the following cases:
 *  - RFP is disabled entirely
 *  - RFP is enabled entirely

 *  - (A) RFP is exempted on the framer and framee
 *  - (B) RFP is exempted on the framer and framee

 *  - (C) RFP is exempted on the framer but not the framee
 *  - (D) RFP is exempted on the framer but not the framee

 *  - (E) RFP is not exempted on the framer nor the framee
 *  - (F) RFP is not exempted on the framer nor the framee
 * 
 *  - (G) RFP is not exempted on the framer but is on the framee
 *  - (H) RFP is not exempted on the framer but is on the framee
 */

"use strict";

requestLongerTimeout(3);

ChromeUtils.defineModuleGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm"
);

// =============================================================================================
// =============================================================================================

async function testTimePrecision(results, expectedResults, extraData) {
  let testDesc = extraData.testDesc;
  let precision = undefined;

  if (!expectedResults.shouldRFPApply) {
    precision = extraData.Unconditional_Precision;
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
const framer_domain = "example.com";
const iframe_domain = "example.org";
const cross_origin_domain = "example.net";
const uri = `https://${framer_domain}/browser/browser/components/resistfingerprinting/test/browser/file_animationapi_iframer.html`;

// The first three variables are defined here; and then set for test banks below.
let extraData = {};
let extraPrefs = {};
let precision = 100;
let expectedResults = {}; // In this test, we don't have explicit expected values, but rather we expect them to be rounded

// ========================================================================================================================
// Create a function that defines all the tests
function addAllTests(extraData_, extraPrefs_) {
  add_task(
    partial(
      defaultsTest,
      uri,
      iframe_domain,
      cross_origin_domain,
      testTimePrecision,
      expectedResults,
      extraData_,
      extraPrefs_
    )
  );

  add_task(
    partial(
      simpleRFPTest,
      uri,
      iframe_domain,
      cross_origin_domain,
      testTimePrecision,
      expectedResults,
      extraData_,
      extraPrefs_
    )
  );

  // (A) RFP is exempted on the framer and framee and each contacts an exempted cross-origin resource
  add_task(
    partial(
      testA,
      uri,
      iframe_domain,
      cross_origin_domain,
      testTimePrecision,
      expectedResults,
      extraData_,
      extraPrefs_
    )
  );

  // (B) RFP is exempted on the framer and framee and each contacts a non-exempted cross-origin resource
  add_task(
    partial(
      testB,
      uri,
      iframe_domain,
      cross_origin_domain,
      testTimePrecision,
      expectedResults,
      extraData_,
      extraPrefs_
    )
  );

  // (C) RFP is exempted on the framer but not the framee and each contacts an exempted cross-origin resource
  add_task(
    partial(
      testC,
      uri,
      iframe_domain,
      cross_origin_domain,
      testTimePrecision,
      expectedResults,
      extraData_,
      extraPrefs_
    )
  );

  // (D) RFP is exempted on the framer but not the framee and each contacts a non-exempted cross-origin resource
  add_task(
    partial(
      testD,
      uri,
      iframe_domain,
      cross_origin_domain,
      testTimePrecision,
      expectedResults,
      extraData_,
      extraPrefs_
    )
  );

  // (E) RFP is not exempted on the framer nor the framee and each contacts an exempted cross-origin resource
  add_task(
    partial(
      testE,
      uri,
      iframe_domain,
      cross_origin_domain,
      testTimePrecision,
      expectedResults,
      extraData_,
      extraPrefs_
    )
  );

  // (F) RFP is not exempted on the framer nor the framee and each contacts a non-exempted cross-origin resource
  add_task(
    partial(
      testF,
      uri,
      iframe_domain,
      cross_origin_domain,
      testTimePrecision,
      expectedResults,
      extraData_,
      extraPrefs_
    )
  );

  // (G) RFP is not exempted on the framer but is on the framee and each contacts an exempted cross-origin resource
  add_task(
    partial(
      testG,
      uri,
      iframe_domain,
      cross_origin_domain,
      testTimePrecision,
      expectedResults,
      extraData_,
      extraPrefs_
    )
  );

  // (H) RFP is not exempted on the framer but is on the framee and each contacts a non-exempted cross-origin resource
  add_task(
    partial(
      testH,
      uri,
      iframe_domain,
      cross_origin_domain,
      testTimePrecision,
      expectedResults,
      extraData_,
      extraPrefs_
    )
  );
}
