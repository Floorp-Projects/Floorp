/**
 * This tests that the canvas is correctly randomized on a popup (not the starting page)
 *
 * Covers the following cases:
 *  - RFP/FPP is disabled entirely
 *  - RFP is enabled entirely, and only in PBM
 *  - FPP is enabled entirely, and only in PBM
 *  - A normal window when FPP is enabled globally and RFP is enabled in PBM, Protections Enabled and Disabled

 *
 *  - (A) RFP is exempted on the maker and popup
 *  - (C) RFP is exempted on the maker but not the popup
 *  - (E) RFP is not exempted on the maker nor the popup
 *  - (G) RFP is not exempted on the maker but is on the popup
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
  let differences = countDifferencesInUint8Arrays(
    result,
    UNMODIFIED_CANVAS_DATA
  );

  Assert.greaterOrEqual(
    differences,
    expectedResults[0],
    `Checking ${testDesc} for canvas randomization - did not see enough random pixels.`
  );
  Assert.lessOrEqual(
    differences,
    expectedResults[1],
    `Checking ${testDesc} for canvas randomization - saw too many random pixels.`
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

// Be sure to always use `let expectedResults = structuredClone(rfpFullyRandomized)` to do a
//   deep copy and avoiding corrupting the original 'const' object
// The first value represents the minimum number of random pixels we should see
// The second, the maximum number of random pixels
const rfpFullyRandomized = [10000, 999999999];
const fppRandomized = [1, 260];
const noRandom = [0, 0];

// Note that the starting page and the popup will be cross-domain from each other, but this test does not check that we inherit the randomizationkey,
// only that the popup is randomized.
const uri = `https://${FRAMER_DOMAIN}/browser/browser/components/resistfingerprinting/test/browser/file_canvas_iframer.html?mode=popup`;

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

// (A) RFP is exempted on the opener and openee
expectedResults = structuredClone(noRandom);
add_task(testA.bind(null, uri, testCanvasRandomization, expectedResults));

// (C) RFP is exempted on the opener but not the openee
expectedResults = structuredClone(rfpFullyRandomized);
add_task(testC.bind(null, uri, testCanvasRandomization, expectedResults));

// (E) RFP is not exempted on the opener nor the openee
expectedResults = structuredClone(rfpFullyRandomized);
add_task(testE.bind(null, uri, testCanvasRandomization, expectedResults));

// (G) RFP is not exempted on the opener but is on the openee
expectedResults = structuredClone(rfpFullyRandomized);
add_task(testG.bind(null, uri, testCanvasRandomization, expectedResults));
