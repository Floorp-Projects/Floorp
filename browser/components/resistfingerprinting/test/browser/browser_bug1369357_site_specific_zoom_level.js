"use strict";

const testPage = "http://example.net/browser/browser/components/resistFingerprinting/test/browser/file_dummy.html";

add_task(async function() {
  let tab1, tab1Zoom, tab2, tab2Zoom, tab3, tab3Zoom;

  tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, testPage);
  FullZoom.enlarge();
  tab1Zoom = ZoomManager.getZoomForBrowser(tab1.linkedBrowser);

  tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, testPage);
  tab2Zoom = ZoomManager.getZoomForBrowser(tab2.linkedBrowser);

  is(tab2Zoom, tab1Zoom, "privacy.resistFingerprinting is false, site-specific zoom level should be enabled");

  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.resistFingerprinting", true]
    ]
  });

  tab3 = await BrowserTestUtils.openNewForegroundTab(gBrowser, testPage);
  tab3Zoom = ZoomManager.getZoomForBrowser(tab3.linkedBrowser);

  isnot(tab3Zoom, tab1Zoom, "privacy.resistFingerprinting is true, site-specific zoom level should be disabled");

  await BrowserTestUtils.removeTab(tab1);
  await BrowserTestUtils.removeTab(tab2);
  await BrowserTestUtils.removeTab(tab3);
});
