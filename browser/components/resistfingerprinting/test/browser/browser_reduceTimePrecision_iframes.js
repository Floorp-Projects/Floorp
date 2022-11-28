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
  // let testDesc = extraData.testDesc;
  // let precision = extraData.precision;
}

const framer_domain = "example.com";
const iframe_domain = "example.org";
const cross_origin_domain = "example.net";
const uri = `https://${framer_domain}/browser/browser/components/resistfingerprinting/test/browser/file_reduceTimePrecision_iframer.html`;

// The first three variables are defined here; and then set for test banks below.
let extraData = {};
let extraPrefs = {};
let precision = 100;
let expectedResults = {}; // In this test, we don't have explicit expected values, but rather we expect them to be rounded

precision = 100;
extraData = {
  precision,
};
extraPrefs = [
  [
    "privacy.resistFingerprinting.reduceTimerPrecision.microseconds",
    precision * 1000,
  ],
];
add_task(
  partial(
    defaultsTest,
    uri,
    iframe_domain,
    cross_origin_domain,
    testTimePrecision,
    expectedResults,
    extraData,
    extraPrefs
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
    extraData,
    extraPrefs
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
    extraData,
    extraPrefs
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
    extraData,
    extraPrefs
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
    extraData,
    extraPrefs
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
    extraData,
    extraPrefs
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
    extraData,
    extraPrefs
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
    extraData,
    extraPrefs
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
    extraData,
    extraPrefs
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
    extraData,
    extraPrefs
  )
);
