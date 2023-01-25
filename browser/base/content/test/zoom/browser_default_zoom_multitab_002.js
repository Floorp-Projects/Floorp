/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_site_specific_global_zoom() {
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  const TEST_PAGE_URL_1 = "http://example.net";
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  const TEST_PAGE_URL_2 = "http://example.net";

  // Prepare the test tabs
  console.log("Adding tab 1");
  let tab1 = BrowserTestUtils.addTab(gBrowser);
  let tabBrowser1 = gBrowser.getBrowserForTab(tab1);
  await FullZoomHelper.selectTabAndWaitForLocationChange(tab1);
  await FullZoomHelper.load(tab1, TEST_PAGE_URL_1);

  console.log("Adding tab 2");
  let tab2 = BrowserTestUtils.addTab(gBrowser);
  let tabBrowser2 = gBrowser.getBrowserForTab(tab2);
  await FullZoomHelper.selectTabAndWaitForLocationChange(tab2);
  await FullZoomHelper.load(tab2, TEST_PAGE_URL_2);

  // 67% global zoom
  await FullZoomHelper.changeDefaultZoom(67);
  let defaultZoom = await FullZoomHelper.getGlobalValue();
  is(defaultZoom, 0.67, "Global zoom is set to 67%");

  // 67% local zoom tab 1
  await FullZoomHelper.selectTabAndWaitForLocationChange(tab1);
  await TestUtils.waitForCondition(() => {
    console.log(
      "Current tab 1 zoom is: ",
      ZoomManager.getZoomForBrowser(tabBrowser1)
    );
    return ZoomManager.getZoomForBrowser(tabBrowser1) == 0.67;
  });
  is(
    ZoomManager.getZoomForBrowser(tabBrowser1),
    0.67,
    "Setting default zoom causes tab 1 (background) to zoom to default zoom."
  );

  // 67% local zoom tab 2
  await FullZoomHelper.selectTabAndWaitForLocationChange(tab2);
  await TestUtils.waitForCondition(() => {
    console.log(
      "Current tab 2 zoom is: ",
      ZoomManager.getZoomForBrowser(tabBrowser2)
    );
    return ZoomManager.getZoomForBrowser(tabBrowser2) == 0.67;
  });
  is(
    ZoomManager.getZoomForBrowser(tabBrowser2),
    0.67,
    "Setting default zoom causes tab 2 (foreground) to zoom to default zoom."
  );

  // 80% site specific zoom
  await FullZoomHelper.selectTabAndWaitForLocationChange(tab1);
  console.log("Enlarging");
  await FullZoom.enlarge();
  await TestUtils.waitForCondition(() => {
    console.log(
      "Current tab 1 zoom is: ",
      ZoomManager.getZoomForBrowser(tabBrowser1)
    );
    return ZoomManager.getZoomForBrowser(tabBrowser1) == 0.8;
  });
  is(
    ZoomManager.getZoomForBrowser(tabBrowser1),
    0.8,
    "Changed local zoom in tab one."
  );

  await FullZoomHelper.selectTabAndWaitForLocationChange(tab2);
  await TestUtils.waitForCondition(() => {
    console.log(
      "Current tab 2 zoom is: ",
      ZoomManager.getZoomForBrowser(tabBrowser2)
    );
    return ZoomManager.getZoomForBrowser(tabBrowser2) == 0.8;
  });
  is(
    ZoomManager.getZoomForBrowser(tabBrowser2),
    0.8,
    "Second tab respects site specific zoom."
  );

  await FullZoomHelper.removeTabAndWaitForLocationChange();
  await FullZoomHelper.removeTabAndWaitForLocationChange();
});
