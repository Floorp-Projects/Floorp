/**
 * This test only tests values in a blob document that is created by the iframe on one domain, then passed
 * to a cross-origin domain to embed.
 * It is a bit different from all the other tests in this series, because instead of the framer doing nothing
 * except frame the framee; the framer creates the blob document, and the framee embeds the blob document.
 *
 * Covers the following cases:
 *  - RFP is disabled entirely
 *  - RFP is enabled entirely
 *  - FPP is enabled entirely

 *
 *  - (A) RFP is exempted on the framer and framee and (if needed) on another cross-origin domain
 *  - (B) RFP is exempted on the framer and framee but is not on another (if needed) cross-origin domain
 *  - (C) RFP is exempted on the framer and (if needed) on another cross-origin domain, but not the framee
 *  - (D) RFP is exempted on the framer but not the framee nor another (if needed) cross-origin domain
 *  - (E) RFP is not exempted on the framer nor the framee but (if needed) is exempted on another cross-origin domain
 *  - (F) RFP is not exempted on the framer nor the framee nor another (if needed) cross-origin domain
 *  - (G) RFP is not exempted on the framer but is on the framee and (if needed) on another cross-origin domain
 *  - (H) RFP is not exempted on the framer nor another (if needed) cross-origin domain but is on the framee
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

const uri = `https://${FRAMER_DOMAIN}/browser/browser/components/resistfingerprinting/test/browser/file_hwconcurrency_blobcrossorigin_iframer.html`;

requestLongerTimeout(2);

let expectedResults = {};

expectedResults = structuredClone(allNotSpoofed);
add_task(defaultsTest.bind(null, uri, testHWConcurrency, expectedResults));

expectedResults = structuredClone(allSpoofed);
add_task(simpleRFPTest.bind(null, uri, testHWConcurrency, expectedResults));

expectedResults = structuredClone(allSpoofed);
add_task(simpleFPPTest.bind(null, uri, testHWConcurrency, expectedResults));

// (A) RFP is exempted on the framer and framee and (if needed) on another cross-origin domain
// In theory this should be Not Spoofed, however, in this test there is a blob: document that
// has a content principal and a reference to the iframe's parent (when Fission is disabled anyway.)
// The blob's principal does not match the parent's principal, so it is up to the blob to determine
// if it should resist fingerprinting on its own.
// It decides _not_ to resist fingerprinting, but only because nsContentUtils::IsURIInList has
// a check `if (!scheme.EqualsLiteral("http") && !scheme.EqualsLiteral("https")) { return false; }`
// We could in theory, modify that check to check the blob's creation uri, and that would work.
// But I am nervous about changing that code.
// expectedResults = structuredClone(allNotSpoofed);
expectedResults = structuredClone(allSpoofed);
add_task(testA.bind(null, uri, testHWConcurrency, expectedResults));

// (B) RFP is exempted on the framer and framee but is not on another (if needed) cross-origin domain
// Same as A above.
//expectedResults = structuredClone(allNotSpoofed);
expectedResults = structuredClone(allSpoofed);
add_task(testB.bind(null, uri, testHWConcurrency, expectedResults));

// (C) RFP is exempted on the framer and (if needed) on another cross-origin domain, but not the framee
expectedResults = structuredClone(allSpoofed);
add_task(testC.bind(null, uri, testHWConcurrency, expectedResults));

// (D) RFP is exempted on the framer but not the framee nor another (if needed) cross-origin domain
expectedResults = structuredClone(allSpoofed);
add_task(testD.bind(null, uri, testHWConcurrency, expectedResults));

// (E) RFP is not exempted on the framer nor the framee but (if needed) is exempted on another cross-origin domain
expectedResults = structuredClone(allSpoofed);
add_task(testE.bind(null, uri, testHWConcurrency, expectedResults));

// (F) RFP is not exempted on the framer nor the framee nor another (if needed) cross-origin domain
expectedResults = structuredClone(allSpoofed);
add_task(testF.bind(null, uri, testHWConcurrency, expectedResults));

// (G) RFP is not exempted on the framer but is on the framee and (if needed) on another cross-origin domain
expectedResults = structuredClone(allSpoofed);
add_task(testG.bind(null, uri, testHWConcurrency, expectedResults));

// (H) RFP is not exempted on the framer nor another (if needed) cross-origin domain but is on the framee
expectedResults = structuredClone(allSpoofed);
add_task(testH.bind(null, uri, testHWConcurrency, expectedResults));

// Test RFP Enabled in PBM and FPP enabled in Normal Browsing Mode
expectedResults = structuredClone(allNotSpoofed);
add_task(
  simpleRFPPBMFPPTest.bind(null, uri, testHWConcurrency, expectedResults)
);
