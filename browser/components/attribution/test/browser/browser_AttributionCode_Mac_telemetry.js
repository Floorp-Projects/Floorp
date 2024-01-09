ChromeUtils.defineESModuleGetters(this, {
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.sys.mjs",
});
const { MacAttribution } = ChromeUtils.importESModule(
  "resource:///modules/MacAttribution.sys.mjs"
);
const { AttributionIOUtils } = ChromeUtils.importESModule(
  "resource:///modules/AttributionCode.sys.mjs"
);
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

add_task(async function test_blank_attribution() {
  // Ensure no attribution information is present
  try {
    await MacAttribution.delAttributionString();
  } catch (ex) {
    // NS_ERROR_DOM_NOT_FOUND_ERR means there was not an attribution
    // string to delete - which we can safely ignore.
    if (
      !(ex instanceof Ci.nsIException) ||
      ex.result != Cr.NS_ERROR_DOM_NOT_FOUND_ERR
    ) {
      throw ex;
    }
  }
  AttributionCode._clearCache();

  const histogram = Services.telemetry.getHistogramById(
    "BROWSER_ATTRIBUTION_ERRORS"
  );

  try {
    // Clear any existing telemetry
    histogram.clear();

    // Try to read the attribution code
    let result = await AttributionCode.getAttrDataAsync();
    Assert.deepEqual(result, {}, "Should be able to get empty result");

    Assert.deepEqual({}, histogram.snapshot().values || {});
  } finally {
    AttributionCode._clearCache();
    histogram.clear();
  }
});

add_task(async function test_no_attribution() {
  const sandbox = sinon.createSandbox();
  let newApplicationPath = MacAttribution.applicationPath + ".test";
  sandbox.stub(MacAttribution, "applicationPath").get(() => newApplicationPath);

  AttributionCode._clearCache();

  const histogram = Services.telemetry.getHistogramById(
    "BROWSER_ATTRIBUTION_ERRORS"
  );
  try {
    // Clear any existing telemetry
    histogram.clear();

    // Try to read the attribution code
    await AttributionCode.getAttrDataAsync();

    let result = await AttributionCode.getAttrDataAsync();
    Assert.deepEqual(result, {}, "Should be able to get empty result");

    Assert.deepEqual({}, histogram.snapshot().values || {});
  } finally {
    AttributionCode._clearCache();
    histogram.clear();
    sandbox.restore();
  }
});
