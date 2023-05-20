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

async function assertCacheExistsAndIsEmpty() {
  // We should have written to the cache, and be able to read back
  // with no errors.
  const histogram = Services.telemetry.getHistogramById(
    "BROWSER_ATTRIBUTION_ERRORS"
  );
  histogram.clear();

  ok(await AttributionIOUtils.exists(AttributionCode.attributionFile.path));
  Assert.deepEqual(
    "",
    new TextDecoder().decode(
      await AttributionIOUtils.read(AttributionCode.attributionFile.path)
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

  let oldExists = AttributionIOUtils.exists;
  let oldWrite = AttributionIOUtils.write;
  try {
    // Clear any existing telemetry
    histogram.clear();

    // Force the file to not exist and then cause a write error.  This is delicate
    // because various background tasks may invoke `IOUtils.writeAtomic` while
    // this test is running.  Be careful to only stub the one call.
    AttributionIOUtils.exists = () => false;
    AttributionIOUtils.write = () => {
      throw new Error("write_error");
    };

    // Try to read the attribution code.
    let result = await AttributionCode.getAttrDataAsync();
    Assert.deepEqual(
      result,
      { content: "content" },
      "Should be able to get a result even if the file doesn't write"
    );

    TelemetryTestUtils.assertHistogram(histogram, INDEX_WRITE_ERROR, 1);
  } finally {
    AttributionIOUtils.exists = oldExists;
    AttributionIOUtils.write = oldWrite;
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
    let str = Services.uuid.generateUUID().toString().toUpperCase();
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
  await IOUtils.setMacXAttr(
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
