/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test default (global) zoom under a variety of conditions:
 * - No setting change => page should load at 100% zoom
 * - Settings change to 120% => page should snap to 120%
 * - increase => ...
 */

"use strict";

add_task(async function test_init_default_zoom() {
  const TEST_PAGE_URL = "data:text/html;charset=utf-8,<body>hello world</body>";

  // Prepare the test tab
  let tab = BrowserTestUtils.addTab(gBrowser);
  let tabBrowser = gBrowser.getBrowserForTab(tab);
  await FullZoomHelper.selectTabAndWaitForLocationChange(tab);
  await FullZoomHelper.load(tab, TEST_PAGE_URL);

  // 100% global zoom
  let defaultZoom = await FullZoomHelper.getGlobalValue();
  is(defaultZoom, 1, "Global zoom is init at 100%");
  // 100% tab zoom
  is(
    ZoomManager.getZoomForBrowser(tabBrowser),
    1,
    "Current zoom is init at 100%"
  );

  await FullZoomHelper.removeTabAndWaitForLocationChange();
});

add_task(async function test_set_default_zoom() {
  const TEST_PAGE_URL = "data:text/html;charset=utf-8,<body>hello world</body>";

  // Prepare the test tab
  let tab = BrowserTestUtils.addTab(gBrowser);
  let tabBrowser = gBrowser.getBrowserForTab(tab);
  await FullZoomHelper.selectTabAndWaitForLocationChange(tab);
  await FullZoomHelper.load(tab, TEST_PAGE_URL);

  // 120% global zoom
  await FullZoomHelper.changeDefaultZoom(120);
  let defaultZoom = await FullZoomHelper.getGlobalValue();
  is(defaultZoom, 1.2, "Global zoom is at 120%");

  // 120% tab zoom
  await TestUtils.waitForCondition(() => ZoomManager.zoom == 1.2);
  is(
    ZoomManager.getZoomForBrowser(tabBrowser),
    1.2,
    "Current zoom matches changed default zoom"
  );

  await FullZoomHelper.removeTabAndWaitForLocationChange();
});

add_task(async function test_enlarge_reduce_rest_local_zoom() {
  const TEST_PAGE_URL = "data:text/html;charset=utf-8,<body>hello world</body>";

  // Prepare the test tab
  let tab = BrowserTestUtils.addTab(gBrowser);
  let tabBrowser = gBrowser.getBrowserForTab(tab);
  await FullZoomHelper.selectTabAndWaitForLocationChange(tab);
  await FullZoomHelper.load(tab, TEST_PAGE_URL);

  // 120% global zoom persists from previous test

  FullZoom.enlarge();

  // 133% tab zoom
  await TestUtils.waitForCondition(() => ZoomManager.zoom == 1.33);
  is(
    ZoomManager.getZoomForBrowser(tabBrowser),
    1.33,
    "Increasing zoom changes zoom of current tab."
  );
  let defaultZoom = await FullZoomHelper.getGlobalValue();
  // 120% global zoom
  is(
    defaultZoom,
    1.2,
    "Increasing zoom of current tab doesn't change default zoom."
  );

  FullZoom.reduce(); // 120% tab zoom
  FullZoom.reduce(); // 110% tab zoom
  FullZoom.reduce(); // 100% tab zoom

  is(
    ZoomManager.getZoomForBrowser(tabBrowser),
    1,
    "Decreasing zoom changes zoom of current tab."
  );
  defaultZoom = await FullZoomHelper.getGlobalValue();
  // 120% global zoom
  is(
    defaultZoom,
    1.2,
    "Decreasing zoom of current tab doesn't change default zoom."
  );

  FullZoom.reset(); // 120% tab zoom
  await TestUtils.waitForCondition(() => ZoomManager.zoom == 1.2);
  is(
    ZoomManager.getZoomForBrowser(tabBrowser),
    1.2,
    "Reseting zoom causes current tab to zoom to default zoom."
  );

  await FullZoomHelper.removeTabAndWaitForLocationChange();
});

add_task(async function test_multidomain_global_zoom() {
  Services.prefs.setBoolPref("browser.zoom.updateBackgroundTabs", true);

  const TEST_PAGE_URL_1 = "https://example.com";
  const TEST_PAGE_URL_2 = "https://example.org";

  // Prepare the test tabs
  let tab1 = BrowserTestUtils.addTab(gBrowser);
  let tabBrowser1 = gBrowser.getBrowserForTab(tab1);
  await FullZoomHelper.selectTabAndWaitForLocationChange(tab1);
  await FullZoomHelper.load(tab1, TEST_PAGE_URL_1);

  let tab2 = BrowserTestUtils.addTab(gBrowser);
  let tabBrowser2 = gBrowser.getBrowserForTab(tab2);
  await FullZoomHelper.selectTabAndWaitForLocationChange(tab2);
  await FullZoomHelper.load(tab2, TEST_PAGE_URL_2);

  // 67% global zoom
  await FullZoomHelper.changeDefaultZoom(67);

  // 67% local zoom tab 1
  await FullZoomHelper.selectTabAndWaitForLocationChange(tab1);
  await TestUtils.waitForCondition(() => ZoomManager.zoom == 0.67);
  is(
    ZoomManager.getZoomForBrowser(tabBrowser1),
    0.67,
    "Setting default zoom causes tab 1 (background) to zoom to default zoom."
  );

  // 67% local zoom tab 2
  await FullZoomHelper.selectTabAndWaitForLocationChange(tab2);
  await TestUtils.waitForCondition(() => ZoomManager.zoom == 0.67);
  is(
    ZoomManager.getZoomForBrowser(tabBrowser2),
    0.67,
    "Setting default zoom causes tab 2 (foreground) to zoom to default zoom."
  );

  FullZoom.enlarge();
  // 80% local zoom tab 2
  await TestUtils.waitForCondition(() => ZoomManager.zoom == 0.8);
  is(
    ZoomManager.getZoomForBrowser(tabBrowser2),
    0.8,
    "Enlarging local zoom of tab 2."
  );

  // 67% local zoom tab 1
  await FullZoomHelper.selectTabAndWaitForLocationChange(tab1);
  await TestUtils.waitForCondition(() => ZoomManager.zoom == 0.67);
  is(
    ZoomManager.getZoomForBrowser(tabBrowser1),
    0.67,
    "Tab 1 is unchanged by tab 2's enlarge call."
  );

  await FullZoomHelper.removeTabAndWaitForLocationChange();
  await FullZoomHelper.removeTabAndWaitForLocationChange();
});

add_task(async function test_site_specific_global_zoom() {
  Services.prefs.setBoolPref("browser.zoom.updateBackgroundTabs", true);
  Services.prefs.setBoolPref("browser.zoom.siteSpecific", true);

  const TEST_PAGE_URL_1 = "https://example.com";
  const TEST_PAGE_URL_2 = "https://example.com";

  // Prepare the test tabs
  let tab1 = BrowserTestUtils.addTab(gBrowser);
  let tabBrowser1 = gBrowser.getBrowserForTab(tab1);
  await FullZoomHelper.selectTabAndWaitForLocationChange(tab1);
  await FullZoomHelper.load(tab1, TEST_PAGE_URL_1);

  let tab2 = BrowserTestUtils.addTab(gBrowser);
  let tabBrowser2 = gBrowser.getBrowserForTab(tab2);
  await FullZoomHelper.selectTabAndWaitForLocationChange(tab2);
  await FullZoomHelper.load(tab2, TEST_PAGE_URL_2);

  // 67% global zoom persists from previous test

  // 67% local zoom tab 1
  await FullZoomHelper.selectTabAndWaitForLocationChange(tab1);
  await TestUtils.waitForCondition(() => ZoomManager.zoom == 0.67);
  is(
    ZoomManager.getZoomForBrowser(tabBrowser1),
    0.67,
    "Setting default zoom causes tab 1 (background) to zoom to default zoom."
  );

  // 67% local zoom tab 2
  await FullZoomHelper.selectTabAndWaitForLocationChange(tab2);
  await TestUtils.waitForCondition(() => ZoomManager.zoom == 0.67);
  is(
    ZoomManager.getZoomForBrowser(tabBrowser2),
    0.67,
    "Setting default zoom causes tab 2 (foreground) to zoom to default zoom."
  );

  // 80% site specific zoom
  await FullZoomHelper.selectTabAndWaitForLocationChange(tab1);
  FullZoom.enlarge();
  await TestUtils.waitForCondition(() => ZoomManager.zoom == 0.8);
  is(
    ZoomManager.getZoomForBrowser(tabBrowser1),
    0.8,
    "Changed local zoom in tab one."
  );

  await FullZoomHelper.selectTabAndWaitForLocationChange(tab2);
  await TestUtils.waitForCondition(() => ZoomManager.zoom == 0.8);
  is(
    ZoomManager.getZoomForBrowser(tabBrowser2),
    0.8,
    "Second tab respects site specific zoom."
  );

  await FullZoomHelper.removeTabAndWaitForLocationChange();
  await FullZoomHelper.removeTabAndWaitForLocationChange();
});
