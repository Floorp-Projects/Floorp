/**
 * This test compares canvas randomization on a parent and an iframe, and ensures that the canvas randomization key
 * is inherited correctly.  (e.g. that the canvases have the same random value)
 *
 * It runs all the tests twice - once for when the iframe is cross-domain, and once when it is same-domain
 *
 * Covers the following cases:
 *  - RFP/FPP is disabled entirely
 *  - RFP is enabled entirely, and only in PBM
 *  - FPP is enabled entirely, and only in PBM
 *  - A normal window when FPP is enabled globally and RFP is enabled in PBM, Protections Enabled and Disabled
 *
 */

"use strict";

// =============================================================================================

/**
 * Compares two Uint8Arrays and returns the number of bits that are different.
 *
 * @param {Uint8ClampedArray} arr1 - The first Uint8ClampedArray to compare.
 * @param {Uint8ClampedArray} arr2 - The second Uint8ClampedArray to compare.
 * @returns {number} - The number of bits that are different between the two
 *                     arrays.
 */
function countDifferencesInUint8Arrays(arr1, arr2) {
  let count = 0;
  for (let i = 0; i < arr1.length; i++) {
    let diff = arr1[i] ^ arr2[i];
    while (diff > 0) {
      count += diff & 1;
      diff >>= 1;
    }
  }
  return count;
}

// =============================================================================================

async function testCanvasRandomization(result, expectedResults, extraData) {
  let testDesc = extraData.testDesc;

  let parent = result.mine;
  let child = result.theirs;

  let differencesInRandom = countDifferencesInUint8Arrays(parent, child);
  let differencesFromUnmodifiedParent = countDifferencesInUint8Arrays(
    UNMODIFIED_CANVAS_DATA,
    parent
  );
  let differencesFromUnmodifiedChild = countDifferencesInUint8Arrays(
    UNMODIFIED_CANVAS_DATA,
    child
  );

  Assert.greaterOrEqual(
    differencesFromUnmodifiedParent,
    expectedResults[0],
    `Checking ${testDesc} for canvas randomization, comparing parent - lower bound for random pixels.`
  );
  Assert.lessOrEqual(
    differencesFromUnmodifiedParent,
    expectedResults[1],
    `Checking ${testDesc} for canvas randomization, comparing parent - upper bound for random pixels.`
  );

  Assert.greaterOrEqual(
    differencesFromUnmodifiedChild,
    expectedResults[0],
    `Checking ${testDesc} for canvas randomization, comparing child - lower bound for random pixels.`
  );
  Assert.lessOrEqual(
    differencesFromUnmodifiedChild,
    expectedResults[1],
    `Checking ${testDesc} for canvas randomization, comparing child - upper bound for random pixels.`
  );

  Assert.greaterOrEqual(
    differencesInRandom,
    expectedResults[2],
    `Checking ${testDesc} and comparing randomization - lower bound for different random pixels.`
  );
  Assert.lessOrEqual(
    differencesInRandom,
    expectedResults[3],
    `Checking ${testDesc} and comparing randomization - upper bound for different random pixels.`
  );
}

requestLongerTimeout(2);

let expectedResults = {};
var UNMODIFIED_CANVAS_DATA = undefined;

add_setup(async function () {
  // Disable the fingerprinting randomization.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.fingerprintingProtection", false],
      ["privacy.fingerprintingProtection.pbmode", false],
      ["privacy.resistFingerprinting", false],
    ],
  });

  let extractCanvasData = function () {
    let offscreenCanvas = new OffscreenCanvas(100, 100);

    const context = offscreenCanvas.getContext("2d");

    // Draw a red rectangle
    context.fillStyle = "#EE2222";
    context.fillRect(0, 0, 100, 100);
    context.fillStyle = "#2222EE";
    context.fillRect(20, 20, 100, 100);

    const imageData = context.getImageData(0, 0, 100, 100);
    return imageData.data;
  };

  function runExtractCanvasData(tab) {
    let code = extractCanvasData.toString();
    return SpecialPowers.spawn(tab.linkedBrowser, [code], async funccode => {
      await content.eval(`var extractCanvasData = ${funccode}`);
      let result = await content.eval(`extractCanvasData()`);
      return result;
    });
  }

  const emptyPage =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.com"
    ) + "empty.html";

  // Open a tab for extracting the canvas data.
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, emptyPage);

  let data = await runExtractCanvasData(tab);
  UNMODIFIED_CANVAS_DATA = data;

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

// Be sure to always use `let expectedResults = structuredClone(allNotSpoofed)` to do a
//   deep copy and avoiding corrupting the original 'const' object
// The first value represents the minimum number of random pixels we should see
// The second, the maximum number of random pixels
// The third, the minimum number of differences between the canvases of the parent and child
// The fourth, the maximum number of differences between the canvases of the parent and child
const rfpFullyRandomized = [10000, 999999999, 20000, 999999999];
const fppRandomized = [1, 260, 0, 0];
const noRandom = [0, 0, 0, 0];

// Note that we are inheriting the randomization key ACROSS top-level domains that are cross-domain, because the iframe is a 3rd party domain
let uri = `https://${FRAMER_DOMAIN}/browser/browser/components/resistfingerprinting/test/browser/file_canvascompare_iframer.html?mode=iframe`;

expectedResults = structuredClone(noRandom);
add_task(
  defaultsTest.bind(null, uri, testCanvasRandomization, expectedResults)
);

expectedResults = structuredClone(fppRandomized);
add_task(
  defaultsPBMTest.bind(null, uri, testCanvasRandomization, expectedResults)
);

expectedResults = structuredClone(rfpFullyRandomized);
add_task(
  simpleRFPTest.bind(null, uri, testCanvasRandomization, expectedResults)
);

// Test a private window with RFP enabled in PBMode
expectedResults = structuredClone(rfpFullyRandomized);
add_task(
  simplePBMRFPTest.bind(null, uri, testCanvasRandomization, expectedResults)
);

expectedResults = structuredClone(fppRandomized);
add_task(
  simpleFPPTest.bind(null, uri, testCanvasRandomization, expectedResults)
);

// Test a Private Window with FPP Enabled in PBM
expectedResults = structuredClone(fppRandomized);
add_task(
  simplePBMFPPTest.bind(null, uri, testCanvasRandomization, expectedResults)
);

// Test RFP Enabled in PBM and FPP enabled in Normal Browsing Mode, No Protections
expectedResults = structuredClone(noRandom);
add_task(
  RFPPBMFPP_NormalMode_NoProtectionsTest.bind(
    null,
    uri,
    testCanvasRandomization,
    expectedResults
  )
);

// Test RFP Enabled in PBM and FPP enabled in Normal Browsing Mode, Protections Enabled
expectedResults = structuredClone(fppRandomized);
add_task(
  RFPPBMFPP_NormalMode_ProtectionsTest.bind(
    null,
    uri,
    testCanvasRandomization,
    expectedResults
  )
);

// And here the we are inheriting the randomization key into an iframe that is same-domain to the parent
uri = `https://${IFRAME_DOMAIN}/browser/browser/components/resistfingerprinting/test/browser/file_canvascompare_iframer.html?mode=iframe`;

expectedResults = structuredClone(noRandom);
add_task(
  defaultsTest.bind(null, uri, testCanvasRandomization, expectedResults)
);

expectedResults = structuredClone(rfpFullyRandomized);
add_task(
  simpleRFPTest.bind(null, uri, testCanvasRandomization, expectedResults)
);

// Test a private window with RFP enabled in PBMode
expectedResults = structuredClone(rfpFullyRandomized);
add_task(
  simplePBMRFPTest.bind(null, uri, testCanvasRandomization, expectedResults)
);

expectedResults = structuredClone(fppRandomized);
add_task(
  simpleFPPTest.bind(null, uri, testCanvasRandomization, expectedResults)
);

// Test a Private Window with FPP Enabled in PBM
expectedResults = structuredClone(fppRandomized);
add_task(
  simplePBMFPPTest.bind(null, uri, testCanvasRandomization, expectedResults)
);

// Test RFP Enabled in PBM and FPP enabled in Normal Browsing Mode, No Protections
expectedResults = structuredClone(noRandom);
add_task(
  RFPPBMFPP_NormalMode_NoProtectionsTest.bind(
    null,
    uri,
    testCanvasRandomization,
    expectedResults
  )
);

// Test RFP Enabled in PBM and FPP enabled in Normal Browsing Mode, Protections Enabled
expectedResults = structuredClone(fppRandomized);
add_task(
  RFPPBMFPP_NormalMode_ProtectionsTest.bind(
    null,
    uri,
    testCanvasRandomization,
    expectedResults
  )
);
