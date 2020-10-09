ChromeUtils.defineModuleGetter(
  this,
  "TelemetryTestUtils",
  "resource://testing-common/TelemetryTestUtils.jsm"
);
const { MacAttribution } = ChromeUtils.import(
  "resource:///modules/MacAttribution.jsm"
);
const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

async function assertCacheExistsAndIsEmpty() {
  // We should have written to the cache, and be able to read back
  // with no errors.
  const histogram = Services.telemetry.getHistogramById(
    "BROWSER_ATTRIBUTION_ERRORS"
  );
  histogram.clear();

  ok(await OS.File.exists(AttributionCode.attributionFile.path));
  Assert.deepEqual(
    "",
    new TextDecoder().decode(
      await OS.File.read(AttributionCode.attributionFile.path)
    )
  );

  AttributionCode._clearCache();
  let result = await AttributionCode.getAttrDataAsync();
  Assert.deepEqual(result, {}, "Should be able to get cached result");

  Assert.deepEqual({}, histogram.snapshot().values || {});
}

add_task(async function test_write_error() {
  const sandbox = sinon.createSandbox();
  let attributionSvc = Cc["@mozilla.org/mac-attribution;1"].getService(
    Ci.nsIMacAttributionService
  );
  attributionSvc.setReferrerUrl(
    MacAttribution.applicationPath,
    "https://example.com?content=content",
    true
  );

  await AttributionCode.deleteFileAsync();
  AttributionCode._clearCache();

  const histogram = Services.telemetry.getHistogramById(
    "BROWSER_ATTRIBUTION_ERRORS"
  );

  try {
    // Clear any existing telemetry
    histogram.clear();

    // Force the file to not exist and then cause a write error.  This is delicate
    // because various background tasks may invoke `OS.File.writeAtomic` while
    // this test is running.  Be careful to only stub the one call.
    const writeAtomic = sandbox.stub(OS.File, "writeAtomic");
    writeAtomic
      .withArgs(
        sinon.match(AttributionCode.attributionFile.path),
        sinon.match.any
      )
      .throws(() => new Error("write_error"));
    OS.File.writeAtomic.callThrough();

    // Try to read the attribution code.
    let result = await AttributionCode.getAttrDataAsync();
    Assert.deepEqual(
      result,
      { content: "content" },
      "Should be able to get a result even if the file doesn't write"
    );

    TelemetryTestUtils.assertHistogram(histogram, INDEX_WRITE_ERROR, 1);
  } finally {
    await AttributionCode.deleteFileAsync();
    AttributionCode._clearCache();
    histogram.clear();
    sandbox.restore();
  }
});

add_task(async function test_unusual_referrer() {
  // This referrer URL looks malformed, but the malformed bits are dropped, so
  // it's actually ok.  This is what allows extraneous bits like `fbclid` tags
  // to be ignored.
  let attributionSvc = Cc["@mozilla.org/mac-attribution;1"].getService(
    Ci.nsIMacAttributionService
  );
  attributionSvc.setReferrerUrl(
    MacAttribution.applicationPath,
    "https://example.com?content=&=campaign",
    true
  );

  await AttributionCode.deleteFileAsync();
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

    await assertCacheExistsAndIsEmpty();
  } finally {
    await AttributionCode.deleteFileAsync();
    AttributionCode._clearCache();
    histogram.clear();
  }
});

add_task(async function test_blank_referrer() {
  let attributionSvc = Cc["@mozilla.org/mac-attribution;1"].getService(
    Ci.nsIMacAttributionService
  );
  attributionSvc.setReferrerUrl(MacAttribution.applicationPath, "", true);

  await AttributionCode.deleteFileAsync();
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

    await assertCacheExistsAndIsEmpty();
  } finally {
    await AttributionCode.deleteFileAsync();
    AttributionCode._clearCache();
    histogram.clear();
  }
});

add_task(async function test_no_referrer() {
  const sandbox = sinon.createSandbox();
  let newApplicationPath = MacAttribution.applicationPath + ".test";
  sandbox.stub(MacAttribution, "applicationPath").get(() => newApplicationPath);

  await AttributionCode.deleteFileAsync();
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

    await assertCacheExistsAndIsEmpty();
  } finally {
    await AttributionCode.deleteFileAsync();
    AttributionCode._clearCache();
    histogram.clear();
    sandbox.restore();
  }
});

add_task(async function test_broken_referrer() {
  let attributionSvc = Cc["@mozilla.org/mac-attribution;1"].getService(
    Ci.nsIMacAttributionService
  );
  attributionSvc.setReferrerUrl(
    MacAttribution.applicationPath,
    "https://example.com?content=content",
    true
  );

  // This uses macOS internals to change the GUID so that it will look like the
  // application has quarantine data but nothing will be pressent in the
  // quarantine database.  This shouldn't happen in the wild.
  function generateQuarantineGUID() {
    let str = Cc["@mozilla.org/uuid-generator;1"]
      .getService(Ci.nsIUUIDGenerator)
      .generateUUID()
      .toString()
      .toUpperCase();
    // Strip {}.
    return str.substring(1, str.length - 1);
  }

  // These magic constants are macOS GateKeeper flags.
  let string = [
    "01c1",
    "5991b778",
    "Safari.app",
    generateQuarantineGUID(),
  ].join(";");
  let bytes = new TextEncoder().encode(string);
  await OS.File.macSetXAttr(
    MacAttribution.applicationPath,
    "com.apple.quarantine",
    bytes
  );

  await AttributionCode.deleteFileAsync();
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

    TelemetryTestUtils.assertHistogram(histogram, INDEX_QUARANTINE_ERROR, 1);
    histogram.clear();

    await assertCacheExistsAndIsEmpty();
  } finally {
    await AttributionCode.deleteFileAsync();
    AttributionCode._clearCache();
    histogram.clear();
  }
});
