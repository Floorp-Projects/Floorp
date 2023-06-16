"use strict";

const PATH_NET = TEST_PATH + "file_dummy.html";
const PATH_ORG = PATH_NET.replace("example.net", "example.org");

add_task(async function () {
  let tab1, tab1Zoom, tab2, tab2Zoom, tab3, tab3Zoom;

  tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, PATH_NET);
  await FullZoom.enlarge();
  tab1Zoom = ZoomManager.getZoomForBrowser(tab1.linkedBrowser);

  tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, PATH_NET);
  tab2Zoom = ZoomManager.getZoomForBrowser(tab2.linkedBrowser);

  is(
    tab2Zoom,
    tab1Zoom,
    "privacy.resistFingerprinting is false, site-specific zoom level should be enabled"
  );

  await SpecialPowers.pushPrefEnv({
    set: [["privacy.resistFingerprinting", true]],
  });

  tab3 = await BrowserTestUtils.openNewForegroundTab(gBrowser, PATH_NET);
  tab3Zoom = ZoomManager.getZoomForBrowser(tab3.linkedBrowser);

  isnot(
    tab3Zoom,
    tab1Zoom,
    "privacy.resistFingerprinting is true, site-specific zoom level should be disabled"
  );

  await FullZoom.reset();

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);

  await SpecialPowers.popPrefEnv();
});

add_task(async function exempt_domain() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PATH_NET);
  await FullZoom.enlarge();
  const netZoomOriginal = ZoomManager.getZoomForBrowser(tab.linkedBrowser);
  is(netZoomOriginal, 1.1, "Initial zoom is 110%");
  await BrowserTestUtils.removeTab(tab);

  tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PATH_ORG);
  await FullZoom.enlarge();
  const orgZoomOriginal = ZoomManager.getZoomForBrowser(tab.linkedBrowser);
  is(orgZoomOriginal, 1.1, "Initial zoom is 110%");
  await BrowserTestUtils.removeTab(tab);

  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.resistFingerprinting.exemptedDomains", "example.net"],
      ["privacy.resistFingerprinting", true],
    ],
  });

  tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PATH_NET);
  const netZoom = ZoomManager.getZoomForBrowser(tab.linkedBrowser);
  is(netZoom, 1.1, "exempted example.net tab should have kept zoom level");
  await FullZoom.reset();
  await BrowserTestUtils.removeTab(tab);

  tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PATH_ORG);
  const orgZoom = ZoomManager.getZoomForBrowser(tab.linkedBrowser);
  is(orgZoom, 1.0, "example.org tab has its zoom reset to default 100%");
  await FullZoom.reset();
  await BrowserTestUtils.removeTab(tab);

  await SpecialPowers.popPrefEnv();
});
