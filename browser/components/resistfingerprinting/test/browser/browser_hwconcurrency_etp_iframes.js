/**
 * This test only tests values in the iframe, it does not test them on the framer
 *
 * Covers the following cases:
 *  - RFP is disabled entirely
 *  - RFP is enabled entirely in normal and PBM
 *  - FPP is enabled entirely in normal and PBM
 *
 */

"use strict";

const SPOOFED_HW_CONCURRENCY = 2;

const DEFAULT_HARDWARE_CONCURRENCY = navigator.hardwareConcurrency;

// =============================================================================================
// =============================================================================================

async function testHWConcurrency(result, expectedResults, extraData) {
  let testDesc = extraData.testDesc;

  is(
    result.hardwareConcurrency,
    expectedResults.hardwareConcurrency,
    `Checking ${testDesc} navigator.hardwareConcurrency.`
  );
}

// The following are convenience objects that allow you to quickly see what is
//   and is not modified from a logical set of values.
// Be sure to always use `let expectedResults = structuredClone(allNotSpoofed)` to do a
//   deep copy and avoiding corrupting the original 'const' object
const allNotSpoofed = {
  hardwareConcurrency: DEFAULT_HARDWARE_CONCURRENCY,
};
const allSpoofed = {
  hardwareConcurrency: SPOOFED_HW_CONCURRENCY,
};

const uri = `https://${FRAMER_DOMAIN}/browser/browser/components/resistfingerprinting/test/browser/file_hwconcurrency_iframer.html?mode=iframe`;

requestLongerTimeout(2);

let extraData = {
  etp_reload: true,
};

let expectedResults = {};

expectedResults = structuredClone(allNotSpoofed);
add_task(
  defaultsTest.bind(null, uri, testHWConcurrency, expectedResults, extraData)
);

// The ETP toggle _does not_ override RFP, only FPP.
expectedResults = structuredClone(allSpoofed);
add_task(
  simpleRFPTest.bind(null, uri, testHWConcurrency, expectedResults, extraData)
);

expectedResults = structuredClone(allSpoofed);
add_task(
  simplePBMRFPTest.bind(
    null,
    uri,
    testHWConcurrency,
    expectedResults,
    extraData
  )
);

expectedResults = structuredClone(allNotSpoofed);
add_task(
  simpleFPPTest.bind(null, uri, testHWConcurrency, expectedResults, extraData)
);

expectedResults = structuredClone(allNotSpoofed);
add_task(
  simplePBMFPPTest.bind(
    null,
    uri,
    testHWConcurrency,
    expectedResults,
    extraData
  )
);

// These test cases exercise the unusual configuration of RFP enabled in PBM and FPP enabled
// in normal browsing mode.
let extraPrefs = [
  ["privacy.resistFingerprinting.pbmode", true],
  ["privacy.fingerprintingProtection", true],
  ["privacy.fingerprintingProtection.overrides", "+NavigatorHWConcurrency"],
];

let this_extraData = structuredClone(extraData);
this_extraData.testDesc =
  "RFP enabled in PBM, FPP enabled globally, testing in normal browsing mode";
expectedResults = allNotSpoofed;
add_task(
  defaultsTest.bind(
    null,
    uri,
    testHWConcurrency,
    structuredClone(expectedResults),
    this_extraData,
    structuredClone(extraPrefs)
  )
);

this_extraData = structuredClone(extraData);
this_extraData.testDesc =
  "RFP enabled in PBM, FPP enabled globally, testing in PBM";
this_extraData.private_window = true;
expectedResults = allSpoofed;
add_task(
  defaultsTest.bind(
    null,
    uri,
    testHWConcurrency,
    structuredClone(expectedResults),
    this_extraData,
    structuredClone(extraPrefs)
  )
);
