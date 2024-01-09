/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

add_task(async () => {
  await setupStubs();
});

/**
 * Test validation of attribution codes,
 * to make sure we reject bad ones and accept good ones.
 */
add_task(async function testValidAttrCodes() {
  let msixCampaignIdStub = sinon.stub(AttributionCode, "msixCampaignId");

  let currentCode = null;
  for (let entry of validAttrCodes) {
    currentCode = entry.code;
    // Attribution for MSIX builds works quite differently than regular Windows
    // builds: the attribution codes come from a Windows API that we've wrapped
    // with an XPCOM class. We don't have a way to inject codes into the build,
    // so instead we mock out our XPCOM class and return the desired values from
    // there. (A potential alternative is to have MSIX tests rely on attribution
    // files, but that more or less invalidates the tests.)
    if (
      AppConstants.platform === "win" &&
      Services.sysinfo.getProperty("hasWinPackageId")
    ) {
      // In real life, the attribution codes returned from Microsoft APIs
      // are not URI encoded, and the AttributionCode code that deals with
      // them expects that - so we have to simulate that as well.
      msixCampaignIdStub.callsFake(async () => decodeURIComponent(currentCode));
    } else if (AppConstants.platform === "macosx") {
      const { MacAttribution } = ChromeUtils.importESModule(
        "resource:///modules/MacAttribution.sys.mjs"
      );

      await MacAttribution.setAttributionString(currentCode);
    } else {
      // non-msix windows
      await AttributionCode.writeAttributionFile(currentCode);
    }
    AttributionCode._clearCache();
    let result = await AttributionCode.getAttrDataAsync();

    Assert.deepEqual(
      result,
      entry.parsed,
      "Parsed code should match expected value, code was: " + currentCode
    );
  }
  AttributionCode._clearCache();

  // Restore the msixCampaignId stub so that other tests don't fail stubbing it
  msixCampaignIdStub.restore();
});

/**
 * Make sure codes with various formatting errors are not seen as valid.
 */
add_task(async function testInvalidAttrCodes() {
  let msixCampaignIdStub = sinon.stub(AttributionCode, "msixCampaignId");
  let currentCode = null;

  for (let code of invalidAttrCodes) {
    currentCode = code;

    if (
      AppConstants.platform === "win" &&
      Services.sysinfo.getProperty("hasWinPackageId")
    ) {
      if (code.includes("not set")) {
        // One of the "invalid" codes that we test is an unescaped one.
        // This is valid for most platforms, but we actually _expect_
        // unescaped codes for MSIX builds, so that particular test is not
        // valid for this case.
        continue;
      }

      msixCampaignIdStub.callsFake(async () => decodeURIComponent(currentCode));
    } else if (AppConstants.platform === "macosx") {
      const { MacAttribution } = ChromeUtils.importESModule(
        "resource:///modules/MacAttribution.sys.mjs"
      );

      await MacAttribution.setAttributionString(currentCode);
    } else {
      // non-msix windows
      await AttributionCode.writeAttributionFile(currentCode);
    }
    AttributionCode._clearCache();
    let result = await AttributionCode.getAttrDataAsync();
    Assert.deepEqual(
      result,
      {},
      "Code should have failed to parse: " + currentCode
    );
  }
  AttributionCode._clearCache();

  // Restore the msixCampaignId stub so that other tests don't fail stubbing it
  msixCampaignIdStub.restore();
});

/**
 * Test the cache by deleting the attribution data file
 * and making sure we still get the expected code.
 */
let condition = {
  // macOS and MSIX attribution codes are not cached by us, thus this test is
  // unnecessary for those builds.
  skip_if: () =>
    (AppConstants.platform === "win" &&
      Services.sysinfo.getProperty("hasWinPackageId")) ||
    AppConstants.platform === "macosx",
};
add_task(condition, async function testDeletedFile() {
  // Set up the test by clearing the cache and writing a valid file.
  await AttributionCode.writeAttributionFile(validAttrCodes[0].code);
  let result = await AttributionCode.getAttrDataAsync();
  Assert.deepEqual(
    result,
    validAttrCodes[0].parsed,
    "The code should be readable directly from the file"
  );

  // Delete the file and make sure we can still read the value back from cache.
  await AttributionCode.deleteFileAsync();
  result = await AttributionCode.getAttrDataAsync();
  Assert.deepEqual(
    result,
    validAttrCodes[0].parsed,
    "The code should be readable from the cache"
  );

  // Clear the cache and check we can't read anything.
  AttributionCode._clearCache();
  result = await AttributionCode.getAttrDataAsync();
  Assert.deepEqual(
    result,
    {},
    "Shouldn't be able to get a code after file is deleted and cache is cleared"
  );
});
