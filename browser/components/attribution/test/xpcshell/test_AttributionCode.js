/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { AttributionCode } = ChromeUtils.import(
  "resource:///modules/AttributionCode.jsm"
);
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

let validAttrCodes = [
  {
    code:
      "source%3Dgoogle.com%26medium%3Dorganic%26campaign%3D(not%20set)%26content%3D(not%20set)",
    parsed: {
      source: "google.com",
      medium: "organic",
      campaign: "(not%20set)",
      content: "(not%20set)",
    },
  },
  {
    code: "source%3Dgoogle.com%26medium%3Dorganic%26campaign%3D%26content%3D",
    parsed: { source: "google.com", medium: "organic" },
  },
  {
    code: "source%3Dgoogle.com%26medium%3Dorganic%26campaign%3D(not%20set)",
    parsed: {
      source: "google.com",
      medium: "organic",
      campaign: "(not%20set)",
    },
  },
  {
    code: "source%3Dgoogle.com%26medium%3Dorganic",
    parsed: { source: "google.com", medium: "organic" },
  },
  { code: "source%3Dgoogle.com", parsed: { source: "google.com" } },
  { code: "medium%3Dgoogle.com", parsed: { medium: "google.com" } },
  { code: "campaign%3Dgoogle.com", parsed: { campaign: "google.com" } },
  { code: "content%3Dgoogle.com", parsed: { content: "google.com" } },
];

let invalidAttrCodes = [
  // Empty string
  "",
  // Not escaped
  "source=google.com&medium=organic&campaign=(not set)&content=(not set)",
  // Too long
  "source%3Dreallyreallyreallyreallyreallyreallyreallyreallyreallylongdomain.com%26medium%3Dorganic%26campaign%3D(not%20set)%26content%3Dalmostexactlyenoughcontenttomakethisstringlongerthanthe200characterlimit",
  // Unknown key name
  "source%3Dgoogle.com%26medium%3Dorganic%26large%3Dgeneticallymodified",
  // Empty key name
  "source%3Dgoogle.com%26medium%3Dorganic%26%3Dgeneticallymodified",
];

async function writeAttributionFile(data) {
  let appDir = Services.dirsvc.get("LocalAppData", Ci.nsIFile);
  let file = appDir.clone();
  file.append(Services.appinfo.vendor || "mozilla");
  file.append(AppConstants.MOZ_APP_NAME);

  await OS.File.makeDir(file.path, { from: appDir.path, ignoreExisting: true });

  file.append("postSigningData");
  await OS.File.writeAtomic(file.path, data);
}

/**
 * Test validation of attribution codes,
 * to make sure we reject bad ones and accept good ones.
 */
add_task(async function testValidAttrCodes() {
  for (let entry of validAttrCodes) {
    AttributionCode._clearCache();
    await writeAttributionFile(entry.code);
    let result = await AttributionCode.getAttrDataAsync();
    Assert.deepEqual(
      result,
      entry.parsed,
      "Parsed code should match expected value, code was: " + entry.code
    );
  }
  AttributionCode._clearCache();
});

/**
 * Make sure codes with various formatting errors are not seen as valid.
 */
add_task(async function testInvalidAttrCodes() {
  for (let code of invalidAttrCodes) {
    AttributionCode._clearCache();
    await writeAttributionFile(code);
    let result = await AttributionCode.getAttrDataAsync();
    Assert.deepEqual(result, {}, "Code should have failed to parse: " + code);
  }
  AttributionCode._clearCache();
});

/**
 * Test the cache by deleting the attribution data file
 * and making sure we still get the expected code.
 */
add_task(async function testDeletedFile() {
  // Set up the test by clearing the cache and writing a valid file.
  await writeAttributionFile(validAttrCodes[0].code);
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
