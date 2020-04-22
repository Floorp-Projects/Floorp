"use strict";

const ZOOM_CHANGE_TOPIC = "browser-fullZoom:location-change";

/**
 * Helper to check the zoom level of the preloaded browser
 */
async function checkPreloadedZoom(level, message) {
  // Clear up any previous preloaded to test a fresh version
  NewTabPagePreloading.removePreloadedBrowser(window);
  NewTabPagePreloading.maybeCreatePreloadedBrowser(window);

  // Wait for zoom handling of preloaded
  const browser = gBrowser.preloadedBrowser;
  await new Promise(resolve =>
    Services.obs.addObserver(function obs(subject) {
      if (subject === browser) {
        Services.obs.removeObserver(obs, ZOOM_CHANGE_TOPIC);
        resolve();
      }
    }, ZOOM_CHANGE_TOPIC)
  );

  is(browser.fullZoom.toFixed(2), level, message);

  // Clean up for other tests
  NewTabPagePreloading.removePreloadedBrowser(window);
}

add_task(async function test_default_zoom() {
  await checkPreloadedZoom(1, "default preloaded zoom is 1");
});

/**
 * Helper to open about:newtab and zoom then check matching preloaded zoom
 */
async function zoomNewTab(changeZoom, message) {
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:newtab"
  );
  changeZoom();
  const level = tab.linkedBrowser.fullZoom.toFixed(2);
  BrowserTestUtils.removeTab(tab);

  // Wait for the the update of the full-zoom content pref value, that happens
  // asynchronously after changing the zoom level.
  let cps2 = Cc["@mozilla.org/content-pref/service;1"].getService(
    Ci.nsIContentPrefService2
  );

  await BrowserTestUtils.waitForCondition(() => {
    return new Promise(resolve => {
      cps2.getByDomainAndName(
        "about:newtab",
        "browser.content.full-zoom",
        null,
        {
          handleResult(pref) {
            resolve(level == pref.value);
          },
          handleCompletion() {
            console.log("handleCompletion");
          },
        }
      );
    });
  });

  await checkPreloadedZoom(level, `${message}: ${level}`);
}

add_task(async function test_preloaded_zoom_out() {
  await zoomNewTab(() => FullZoom.reduce(), "zoomed out applied to preloaded");
});

add_task(async function test_preloaded_zoom_in() {
  await zoomNewTab(() => {
    FullZoom.enlarge();
    FullZoom.enlarge();
  }, "zoomed in applied to preloaded");
});

add_task(async function test_preloaded_zoom_default() {
  await zoomNewTab(
    () => FullZoom.reduce(),
    "zoomed back to default applied to preloaded"
  );
});
