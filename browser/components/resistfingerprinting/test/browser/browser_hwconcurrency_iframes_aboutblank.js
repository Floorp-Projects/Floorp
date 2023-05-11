/**
 * This test only tests values in an about:blank document that is created by the iframe, it does not test them on the framer
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
// Be sure to always use `let expectedResults = JSON.parse(JSON.stringify(allNotSpoofed))` to do a
//   deep copy and avoiding corrupting the original 'const' object
const allNotSpoofed = {
  hardwareConcurrency: DEFAULT_HARDWARE_CONCURRENCY,
};
const allSpoofed = {
  hardwareConcurrency: SPOOFED_HW_CONCURRENCY,
};

const FRAMER_DOMAIN = "example.com";
const IFRAME_DOMAIN = "example.org";
const CROSS_ORIGIN_DOMAIN = "example.net";
const uri = `https://${FRAMER_DOMAIN}/browser/browser/components/resistfingerprinting/test/browser/file_hwconcurrency_aboutblank_iframer.html`;

requestLongerTimeout(2);

let expectedResults = {};

expectedResults = JSON.parse(JSON.stringify(allNotSpoofed));
add_task(
  partial(
    defaultsTest,
    uri,
    IFRAME_DOMAIN,
    CROSS_ORIGIN_DOMAIN,
    testHWConcurrency,
    expectedResults
  )
);

expectedResults = JSON.parse(JSON.stringify(allSpoofed));
add_task(
  partial(
    simpleRFPTest,
    uri,
    IFRAME_DOMAIN,
    CROSS_ORIGIN_DOMAIN,
    testHWConcurrency,
    expectedResults
  )
);

// (A) RFP is exempted on the framer and framee and each contacts an exempted cross-origin resource
expectedResults = JSON.parse(JSON.stringify(allNotSpoofed));
add_task(
  partial(
    testA,
    uri,
    IFRAME_DOMAIN,
    CROSS_ORIGIN_DOMAIN,
    testHWConcurrency,
    expectedResults
  )
);

// (B) RFP is exempted on the framer and framee and each contacts a non-exempted cross-origin resource
expectedResults = JSON.parse(JSON.stringify(allNotSpoofed));
add_task(
  partial(
    testB,
    uri,
    IFRAME_DOMAIN,
    CROSS_ORIGIN_DOMAIN,
    testHWConcurrency,
    expectedResults
  )
);

// (C) RFP is exempted on the framer but not the framee and each contacts an exempted cross-origin resource
expectedResults = JSON.parse(JSON.stringify(allSpoofed));
add_task(
  partial(
    testC,
    uri,
    IFRAME_DOMAIN,
    CROSS_ORIGIN_DOMAIN,
    testHWConcurrency,
    expectedResults
  )
);

// (D) RFP is exempted on the framer but not the framee and each contacts a non-exempted cross-origin resource
expectedResults = JSON.parse(JSON.stringify(allSpoofed));
add_task(
  partial(
    testD,
    uri,
    IFRAME_DOMAIN,
    CROSS_ORIGIN_DOMAIN,
    testHWConcurrency,
    expectedResults
  )
);

// (E) RFP is not exempted on the framer nor the framee and each contacts an exempted cross-origin resource
expectedResults = JSON.parse(JSON.stringify(allSpoofed));
add_task(
  partial(
    testE,
    uri,
    IFRAME_DOMAIN,
    CROSS_ORIGIN_DOMAIN,
    testHWConcurrency,
    expectedResults
  )
);

// (F) RFP is not exempted on the framer nor the framee and each contacts a non-exempted cross-origin resource
expectedResults = JSON.parse(JSON.stringify(allSpoofed));
add_task(
  partial(
    testF,
    uri,
    IFRAME_DOMAIN,
    CROSS_ORIGIN_DOMAIN,
    testHWConcurrency,
    expectedResults
  )
);

// (G) RFP is not exempted on the framer but is on the framee and each contacts an exempted cross-origin resource
expectedResults = JSON.parse(JSON.stringify(allSpoofed));
add_task(
  partial(
    testG,
    uri,
    IFRAME_DOMAIN,
    CROSS_ORIGIN_DOMAIN,
    testHWConcurrency,
    expectedResults
  )
);

// (H) RFP is not exempted on the framer but is on the framee and each contacts a non-exempted cross-origin resource
expectedResults = JSON.parse(JSON.stringify(allSpoofed));
add_task(
  partial(
    testH,
    uri,
    IFRAME_DOMAIN,
    CROSS_ORIGIN_DOMAIN,
    testHWConcurrency,
    expectedResults
  )
);
