const baseURL = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

const testFileURL = `${baseURL}file_lazyload_telemetry.html`;

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

function dataIsReported() {
  const snapshot = Services.telemetry.getSnapshotForHistograms("main", false)
    .content;
  return (
    snapshot.LAZYLOAD_IMAGE_TOTAL && snapshot.LAZYLOAD_IMAGE_TOTAL.values[4]
  );
}

add_task(async function testTelemetryCollection() {
  Services.telemetry.getHistogramById("LAZYLOAD_IMAGE_TOTAL").clear();
  Services.telemetry.getHistogramById("LAZYLOAD_IMAGE_STARTED").clear();
  Services.telemetry.getHistogramById("LAZYLOAD_IMAGE_NOT_VIEWPORT").clear();
  Services.telemetry
    .getHistogramById("LAZYLOAD_IMAGE_VIEWPORT_LOADING")
    .clear();
  Services.telemetry.getHistogramById("LAZYLOAD_IMAGE_VIEWPORT_LOADED").clear();

  const testTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    testFileURL,
    true
  );

  await SpecialPowers.spawn(testTab.linkedBrowser.browsingContext, [], () => {
    const image2 = content.document.getElementById("image2");
    image2.scrollIntoView();
  });

  gBrowser.removeTab(testTab);

  await BrowserTestUtils.waitForCondition(dataIsReported);

  const snapshot = Services.telemetry.getSnapshotForHistograms("main", false)
    .content;

  // Ensures we have 4 lazyload images.
  is(snapshot.LAZYLOAD_IMAGE_TOTAL.values[4], 1);
  // All 4 images should be lazy-loaded.
  is(snapshot.LAZYLOAD_IMAGE_STARTED.values[4], 1);
  // The last image didn't reach to the viewport.
  is(snapshot.LAZYLOAD_IMAGE_NOT_VIEWPORT.values[1], 1);
  // The sum of the images that reached to the viewport
  // should be three. This includes all images except
  // the last one.
  is(
    snapshot.LAZYLOAD_IMAGE_VIEWPORT_LOADING.sum +
      snapshot.LAZYLOAD_IMAGE_VIEWPORT_LOADED.sum,
    3
  );
});
