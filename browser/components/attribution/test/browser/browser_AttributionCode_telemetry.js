ChromeUtils.defineModuleGetter(
  this,
  "TelemetryTestUtils",
  "resource://testing-common/TelemetryTestUtils.jsm"
);
const { AttributionCode } = ChromeUtils.import(
  "resource:///modules/AttributionCode.jsm"
);
ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

async function writeAttributionFile(data) {
  let appDir = Services.dirsvc.get("LocalAppData", Ci.nsIFile);
  let file = appDir.clone();
  file.append(Services.appinfo.vendor || "mozilla");
  file.append(AppConstants.MOZ_APP_NAME);

  await OS.File.makeDir(file.path, { from: appDir.path, ignoreExisting: true });

  file.append("postSigningData");
  await OS.File.writeAtomic(file.path, data);
}

add_task(function setup() {
  // Clear cache call is only possible in a testing environment
  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  env.set("XPCSHELL_TEST_PROFILE_DIR", "testing");

  registerCleanupFunction(() => {
    env.set("XPCSHELL_TEST_PROFILE_DIR", null);
  });
});

add_task(async function test_parse_error() {
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
  await writeAttributionFile(""); // empty string is invalid
  result = await AttributionCode.getAttrDataAsync();
  Assert.deepEqual(result, {}, "Should have failed to parse");

  // `assertHistogram` also ensures that `read_error` index 0 is 0
  // as we should not have recorded telemetry from the previous `getAttrDataAsync` call
  TelemetryTestUtils.assertHistogram(histogram, 1, 1);
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

  // Force a read error
  const stub = sandbox.stub(OS.File, "read");
  stub.throws(() => new Error("read_error"));
  // Try to read the file
  await AttributionCode.getAttrDataAsync();

  // It should record the read error
  TelemetryTestUtils.assertHistogram(histogram, 0, 1);

  // Clear any existing telemetry
  histogram.clear();
  sandbox.restore();
});
