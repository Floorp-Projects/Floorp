/**
 * This test only tests values in a blob document that is opened in a popup
 * Because there is no interaction with a third party domain, there's a lot fewer tests
 *
 * Covers the following cases:
 *  - RFP is disabled entirely
 *  - RFP is enabled entirely
 *  - FPP is enabled entirely

 *
 *  - (A) RFP is exempted on the popup maker
 *  - (E) RFP is not exempted on the popup maker
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

const uri = `https://${FRAMER_DOMAIN}/browser/browser/components/resistfingerprinting/test/browser/file_hwconcurrency_blob_popupmaker.html?submode=noopener`;
const await_uri = loadedURL => loadedURL.startsWith("blob:");

requestLongerTimeout(2);

let extraData = {
  noopener: true,
  await_uri,
};

let expectedResults = {};

expectedResults = structuredClone(allNotSpoofed);
add_task(
  defaultsTest.bind(null, uri, testHWConcurrency, expectedResults, extraData)
);

expectedResults = structuredClone(allSpoofed);
add_task(
  simpleRFPTest.bind(null, uri, testHWConcurrency, expectedResults, extraData)
);

expectedResults = structuredClone(allSpoofed);
add_task(
  simpleFPPTest.bind(null, uri, testHWConcurrency, expectedResults, extraData)
);

// (A) RFP is exempted on the popup maker
//     Ordinarily, RFP would be exempted, however because the opener relationship is severed
//     there is nothing to grant it an exemption, so it is not exempted.
expectedResults = structuredClone(allSpoofed);
add_task(testA.bind(null, uri, testHWConcurrency, expectedResults, extraData));

// (E) RFP is not exempted on the popup maker
expectedResults = structuredClone(allSpoofed);
add_task(testE.bind(null, uri, testHWConcurrency, expectedResults, extraData));

// Test RFP Enabled in PBM and FPP enabled in Normal Browsing Mode
expectedResults = structuredClone(allNotSpoofed);
add_task(
  simpleRFPPBMFPPTest.bind(
    null,
    uri,
    testHWConcurrency,
    expectedResults,
    extraData
  )
);
