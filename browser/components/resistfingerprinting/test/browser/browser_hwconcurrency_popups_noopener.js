/**
 * This test tests values in a popup that is opened with noopener, it does not test them on the page that made the popup
 *
 * Covers the following cases:
 *  - RFP is disabled entirely
 *  - RFP is enabled entirely
 *  - FPP is enabled entirely
 *
 *  - (A) RFP is exempted on the maker and popup
 *  - (C) RFP is exempted on the maker but not the popup
 *  - (E) RFP is not exempted on the maker nor the popup
 *  - (G) RFP is not exempted on the maker but is on the popup
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

const uri = `https://${FRAMER_DOMAIN}/browser/browser/components/resistfingerprinting/test/browser/file_hwconcurrency_iframer.html?mode=popup&submode=noopener`;
const await_uri = `https://${IFRAME_DOMAIN}/browser/browser/components/resistfingerprinting/test/browser/file_hwconcurrency_iframee.html?mode=popup`;

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

// (A) RFP is exempted on the maker and popup
expectedResults = structuredClone(allNotSpoofed);
add_task(testA.bind(null, uri, testHWConcurrency, expectedResults, extraData));

// (C) RFP is exempted on the maker but not the popup
expectedResults = structuredClone(allSpoofed);
add_task(testC.bind(null, uri, testHWConcurrency, expectedResults, extraData));

// (E) RFP is not exempted on the maker nor the popup
expectedResults = structuredClone(allSpoofed);
add_task(testE.bind(null, uri, testHWConcurrency, expectedResults, extraData));

// (G) RFP is not exempted on the maker but is on the popup
//     Ordinarily, RFP would not be exempted, however because the opener relationship is severed
//     it is safe to exempt the popup
expectedResults = structuredClone(allNotSpoofed);
add_task(testG.bind(null, uri, testHWConcurrency, expectedResults, extraData));

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
