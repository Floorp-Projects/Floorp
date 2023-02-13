/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { MacAttribution } = ChromeUtils.importESModule(
  "resource:///modules/MacAttribution.sys.mjs"
);

add_task(async () => {
  await setupStubs();
});

add_task(async function testValidAttrCodes() {
  let appPath = MacAttribution.applicationPath;
  let attributionSvc = Cc["@mozilla.org/mac-attribution;1"].getService(
    Ci.nsIMacAttributionService
  );

  for (let entry of validAttrCodes) {
    if (entry.platforms && !entry.platforms.includes("mac")) {
      continue;
    }

    // Set a url referrer.  In the macOS quarantine database, the
    // referrer URL has components that areURI-encoded.  Our test data
    // URI-encodes the components and also the separators (?, &, =).
    // So we decode it and re-encode it to leave just the components
    // URI-encoded.
    let url = `http://example.com?${encodeURI(decodeURIComponent(entry.code))}`;
    attributionSvc.setReferrerUrl(appPath, url, true);
    let referrer = await MacAttribution.getReferrerUrl(appPath);
    equal(referrer, url, "overwrite referrer url");

    // Read attribution code from referrer.
    await AttributionCode.deleteFileAsync();
    AttributionCode._clearCache();
    let result = await AttributionCode.getAttrDataAsync();
    Assert.deepEqual(
      result,
      entry.parsed,
      "Parsed code should match expected value, code was: " + entry.code
    );

    // Read attribution code from file.
    AttributionCode._clearCache();
    result = await AttributionCode.getAttrDataAsync();
    Assert.deepEqual(
      result,
      entry.parsed,
      "Parsed code should match expected value, code was: " + entry.code
    );

    // Does not overwrite cached existing attribution code.
    attributionSvc.setReferrerUrl(appPath, "http://test.com", false);
    referrer = await MacAttribution.getReferrerUrl(appPath);
    equal(referrer, url, "update referrer url");

    result = await AttributionCode.getAttrDataAsync();
    Assert.deepEqual(
      result,
      entry.parsed,
      "Parsed code should match expected value, code was: " + entry.code
    );
  }
});

add_task(async function testInvalidAttrCodes() {
  let appPath = MacAttribution.applicationPath;
  let attributionSvc = Cc["@mozilla.org/mac-attribution;1"].getService(
    Ci.nsIMacAttributionService
  );

  for (let code of invalidAttrCodes) {
    // Set a url referrer.  Not all of these invalid codes can be represented
    // in the quarantine database; skip those ones.
    let url = `http://example.com?${code}`;
    let referrer;
    try {
      attributionSvc.setReferrerUrl(appPath, url, true);
      referrer = await MacAttribution.getReferrerUrl(appPath);
    } catch (ex) {
      continue;
    }
    if (!referrer) {
      continue;
    }
    equal(referrer, url, "overwrite referrer url");

    // Read attribution code from referrer.
    await AttributionCode.deleteFileAsync();
    AttributionCode._clearCache();
    let result = await AttributionCode.getAttrDataAsync();
    Assert.deepEqual(result, {}, "Code should have failed to parse: " + code);
  }
});
