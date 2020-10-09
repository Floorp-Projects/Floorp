/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

/**
 * This test file exists to be run on any platform during development,
 * whereas the test_AttributionCode.js will test the attribution file
 * in the app local data dir on Windows.  It will only run under
 * Windows on try.
 */

/**
 * Test validation of attribution codes.
 */
add_task(async function testValidAttrCodes() {
  for (let entry of validAttrCodes) {
    let result = AttributionCode.parseAttributionCode(entry.code);
    Assert.deepEqual(
      result,
      entry.parsed,
      "Parsed code should match expected value, code was: " + entry.code
    );
  }
});

/**
 * Make sure codes with various formatting errors are not seen as valid.
 */
add_task(async function testInvalidAttrCodes() {
  for (let code of invalidAttrCodes) {
    let result = AttributionCode.parseAttributionCode(code);
    Assert.deepEqual(result, {}, "Code should have failed to parse: " + code);
  }
});
