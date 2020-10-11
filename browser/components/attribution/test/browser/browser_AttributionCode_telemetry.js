ChromeUtils.defineModuleGetter(
  this,
  "TelemetryTestUtils",
  "resource://testing-common/TelemetryTestUtils.jsm"
);
const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

add_task(async function test_parse_error() {
  if (AppConstants.platform == "macosx") {
    // On macOS, the underlying data is the OS-level quarantine
    // database.  We need to start from nothing to isolate the cache.
    const { MacAttribution } = ChromeUtils.import(
      "resource:///modules/MacAttribution.jsm"
    );
    let attributionSvc = Cc["@mozilla.org/mac-attribution;1"].getService(
      Ci.nsIMacAttributionService
    );
    attributionSvc.setReferrerUrl(MacAttribution.applicationPath, "", true);
  }

  registerCleanupFunction(async () => {
    await AttributionCode.deleteFileAsync();
    AttributionCode._clearCache();
  });
  const histogram = Services.telemetry.getHistogramById(
    "BROWSER_ATTRIBUTION_ERRORS"
  );
  // Delete the file to trigger a read error
  await AttributionCode.deleteFileAsync();
  AttributionCode._clearCache();
  // Clear any existing telemetry
  histogram.clear();
  let result = await AttributionCode.getAttrDataAsync();
  Assert.deepEqual(
    result,
    {},
    "Shouldn't be able to get a result if the file doesn't exist"
  );

  // Write an invalid file to trigger a decode error
  await AttributionCode.deleteFileAsync();
  AttributionCode._clearCache();
  // Empty string is valid on macOS.
  await AttributionCode.writeAttributionFile(
    AppConstants.platform == "macosx" ? "invalid" : ""
  );
  result = await AttributionCode.getAttrDataAsync();
  Assert.deepEqual(result, {}, "Should have failed to parse");

  // `assertHistogram` also ensures that `read_error` index 0 is 0
  // as we should not have recorded telemetry from the previous `getAttrDataAsync` call
  TelemetryTestUtils.assertHistogram(histogram, INDEX_DECODE_ERROR, 1);
  // Reset
  histogram.clear();
});

add_task(async function test_read_error() {
  registerCleanupFunction(async () => {
    await AttributionCode.deleteFileAsync();
    AttributionCode._clearCache();
  });
  const histogram = Services.telemetry.getHistogramById(
    "BROWSER_ATTRIBUTION_ERRORS"
  );
  const sandbox = sinon.createSandbox();
  // Delete the file to trigger a read error
  await AttributionCode.deleteFileAsync();
  AttributionCode._clearCache();
  // Clear any existing telemetry
  histogram.clear();

  // Force the file to exist but then cause a read error
  const exists = sandbox.stub(OS.File, "exists");
  exists.resolves(true);
  const read = sandbox.stub(OS.File, "read");
  read.throws(() => new Error("read_error"));
  // Try to read the file
  await AttributionCode.getAttrDataAsync();

  // It should record the read error
  TelemetryTestUtils.assertHistogram(histogram, INDEX_READ_ERROR, 1);

  // Clear any existing telemetry
  histogram.clear();
  sandbox.restore();
});
