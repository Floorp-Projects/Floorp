const baseURL = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

const testFileURL = `${baseURL}file_lazyload_telemetry.html`;

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

function OtherLazyLoadDataIsReported() {
  const snapshot = Services.telemetry.getSnapshotForHistograms("main", false)
    .content;
  return snapshot.LAZYLOAD_IMAGE_TOTAL;
}

function pageLoadIsReported() {
  const snapshot = Services.telemetry.getSnapshotForHistograms("main", false)
    .parent;
  return snapshot.FX_LAZYLOAD_IMAGE_PAGE_LOAD_MS;
}

add_task(async function testTelemetryCollection() {
  Services.telemetry.getHistogramById("FX_LAZYLOAD_IMAGE_PAGE_LOAD_MS").clear();

  const testTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "data:text/html,<body><iframe src='" + testFileURL + "'></iframe></body>",
    true
  );

  await BrowserTestUtils.waitForCondition(pageLoadIsReported);

  gBrowser.removeTab(testTab);

  // Running this test also causes some other LAZYLOAD related data
  // to be collected. Wait for them to be collected to avoid firing
  // them at an unexpected time.
  await BrowserTestUtils.waitForCondition(OtherLazyLoadDataIsReported);

  const snapshot = Services.telemetry.getSnapshotForHistograms("main", false);

  ok(
    snapshot.parent.FX_LAZYLOAD_IMAGE_PAGE_LOAD_MS.sum > 0,
    "lazyload image page load telemetry"
  );
});
