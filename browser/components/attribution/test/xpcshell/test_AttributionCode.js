/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

add_task(async () => {
  await setupStubs();
});

/**
 * Test validation of attribution codes,
 * to make sure we reject bad ones and accept good ones.
 */
add_task(async function testValidAttrCodes() {
  for (let entry of validAttrCodes) {
    AttributionCode._clearCache();
    await AttributionCode.writeAttributionFile(entry.code);
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
    await AttributionCode.writeAttributionFile(code);
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
