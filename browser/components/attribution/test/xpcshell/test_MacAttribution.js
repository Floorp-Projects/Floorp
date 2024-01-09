/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { MacAttribution } = ChromeUtils.importESModule(
  "resource:///modules/MacAttribution.sys.mjs"
);

let extendedAttributeTestCases = [
  {
    raw: "__MOZCUSTOM__dlsource%3D=mozci",
    expected: "dlsource%3D=mozci",
    error: false,
  },
  {
    raw: "__MOZCUSTOM__dlsource%3D=mozci\0\0\0\0\0",
    expected: "dlsource%3D=mozci",
    error: false,
  },
  {
    raw: "__MOZCUSTOM__dlsource%3D=mozci\t\t\t\t\t",
    expected: "dlsource%3D=mozci",
    error: false,
  },
  {
    raw: "__MOZCUSTOM__dlsource%3D=mozci\0\0\0\t\t",
    expected: "dlsource%3D=mozci",
    error: false,
  },
  {
    raw: "__MOZCUSTOM__dlsource%3D=mozci\t\t\0\0",
    expected: "dlsource%3D=mozci",
    error: false,
  },
  {
    raw: "__MOZCUSTOM__dlsource%3D=mozci\0\t\0\t\0\t",
    expected: "dlsource%3D=mozci",
    error: false,
  },
  {
    raw: "",
    expected: "",
    error: true,
  },
  {
    raw: "dlsource%3D=mozci\0\t\0\t\0\t",
    expected: "",
    error: true,
  },
];

add_task(async () => {
  await setupStubs();
});

// Tests to ensure that MacAttribution.getAttributionString
// strips away the parts of the extended attribute that it should,
// and that invalid extended attribute values result in no attribution
// data.
add_task(async function testExtendedAttributeProcessing() {
  for (let entry of extendedAttributeTestCases) {
    // We use setMacXAttr directly here rather than setAttributionString because
    // we need the ability to set invalid attribution strings.
    await IOUtils.setMacXAttr(
      MacAttribution.applicationPath,
      "com.apple.application-instance",
      new TextEncoder().encode(entry.raw)
    );
    try {
      let got = await MacAttribution.getAttributionString();
      if (entry.error === true) {
        Assert.ok(false, "Expected error, raw code was: " + entry.raw);
      }
      Assert.equal(
        got,
        entry.expected,
        "Returned code should match expected value, raw code was: " + entry.raw
      );
    } catch (err) {
      if (entry.error === false) {
        Assert.ok(
          false,
          "Unexpected error, raw code was: " + entry.raw + " error is: " + err
        );
      }
    }
  }
});

add_task(async function testValidAttrCodes() {
  for (let entry of validAttrCodes) {
    if (entry.platforms && !entry.platforms.includes("mac")) {
      continue;
    }

    await MacAttribution.setAttributionString(entry.code);

    // Read attribution code from xattr.
    AttributionCode._clearCache();
    let result = await AttributionCode.getAttrDataAsync();
    Assert.deepEqual(
      result,
      entry.parsed,
      "Parsed code should match expected value, code was: " + entry.code
    );

    // Read attribution code from cache.
    result = await AttributionCode.getAttrDataAsync();
    Assert.deepEqual(
      result,
      entry.parsed,
      "Parsed code should match expected value, code was: " + entry.code
    );

    // Does not overwrite cached existing attribution code.
    await MacAttribution.setAttributionString("__MOZCUSTOM__testcode");
    result = await AttributionCode.getAttrDataAsync();
    Assert.deepEqual(
      result,
      entry.parsed,
      "Parsed code should match expected value, code was: " + entry.code
    );
  }
});

add_task(async function testInvalidAttrCodes() {
  for (let code of invalidAttrCodes) {
    await MacAttribution.setAttributionString("__MOZCUSTOM__" + code);

    // Read attribution code from xattr
    AttributionCode._clearCache();
    let result = await AttributionCode.getAttrDataAsync();
    Assert.deepEqual(result, {}, "Code should have failed to parse: " + code);
  }
});
