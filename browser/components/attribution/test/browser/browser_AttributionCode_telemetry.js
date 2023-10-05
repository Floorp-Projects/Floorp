ChromeUtils.defineESModuleGetters(this, {
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.sys.mjs",
});
const { AttributionIOUtils } = ChromeUtils.importESModule(
  "resource:///modules/AttributionCode.sys.mjs"
);

add_task(async function test_parse_error() {
  if (AppConstants.platform == "macosx") {
    const { MacAttribution } = ChromeUtils.importESModule(
      "resource:///modules/MacAttribution.sys.mjs"
    );
    MacAttribution.setAttributionString("");
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
  // Skip this for MSIX packages though - we can't write or delete
  // the attribution file there, everything happens in memory instead.
  if (
    AppConstants.platform === "win" &&
    !Services.sysinfo.getProperty("hasWinPackageId")
  ) {
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
  }
});

add_task(async function test_read_error() {
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

  // Force the file to exist but then cause a read error
  let oldExists = AttributionIOUtils.exists;
  AttributionIOUtils.exists = () => true;

  let oldRead = AttributionIOUtils.read;
  AttributionIOUtils.read = () => {
    throw new Error("read_error");
  };

  registerCleanupFunction(() => {
    AttributionIOUtils.exists = oldExists;
    AttributionIOUtils.read = oldRead;
  });

  // Try to read the file
  await AttributionCode.getAttrDataAsync();

  // It should record the read error
  TelemetryTestUtils.assertHistogram(histogram, INDEX_READ_ERROR, 1);

  // Clear any existing telemetry
  histogram.clear();
});
