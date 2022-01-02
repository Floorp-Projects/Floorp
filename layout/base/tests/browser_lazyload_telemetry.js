const baseURL = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

const testFileURL = `${baseURL}file_lazyload_telemetry.html`;

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

function pageLoadIsReported() {
  const snapshot = Services.telemetry.getSnapshotForHistograms("main", false)
    .parent;
  return snapshot.FX_LAZYLOAD_IMAGE_PAGE_LOAD_MS;
}

function dataIsReported() {
  const snapshot = Services.telemetry.getSnapshotForHistograms("main", false)
    .content;
  return (
    snapshot.LAZYLOAD_IMAGE_STARTED && snapshot.LAZYLOAD_IMAGE_STARTED.values[4]
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
  Services.telemetry.getHistogramById("FX_LAZYLOAD_IMAGE_PAGE_LOAD_MS").clear();

  const testTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    testFileURL,
    true
  );

  await SpecialPowers.spawn(
    testTab.linkedBrowser.browsingContext,
    [],
    async () => {
      const image2 = content.document.getElementById("image2");
      image2.scrollIntoView();

      await new Promise(resolve => {
        content.requestAnimationFrame(() => {
          content.requestAnimationFrame(resolve);
        });
      });
    }
  );

  await BrowserTestUtils.waitForCondition(pageLoadIsReported);

  gBrowser.removeTab(testTab);

  await BrowserTestUtils.waitForCondition(dataIsReported);

  const snapshot = Services.telemetry.getSnapshotForHistograms("main", false);

  // Ensures we have 4 lazyload images.
  is(snapshot.content.LAZYLOAD_IMAGE_TOTAL.values[4], 1, "total images");
  // All 4 images should be lazy-loaded.
  is(snapshot.content.LAZYLOAD_IMAGE_STARTED.values[4], 1, "started to load");
  // The last image didn't reach to the viewport.
  is(
    snapshot.content.LAZYLOAD_IMAGE_NOT_VIEWPORT.values[1],
    1,
    "images didn't reach viewport"
  );
  // The sum of the images that reached to the viewport
  // should be three. This includes all images except
  // the last one.
  is(
    snapshot.content.LAZYLOAD_IMAGE_VIEWPORT_LOADING.sum +
      snapshot.content.LAZYLOAD_IMAGE_VIEWPORT_LOADED.sum,
    3,
    "images reached viewport"
  );
  ok(
    snapshot.parent.FX_LAZYLOAD_IMAGE_PAGE_LOAD_MS.sum > 0,
    "lazyload image page load telemetry"
  );
});
