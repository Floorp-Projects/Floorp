/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_init_default_zoom() {
  const TEST_PAGE_URL =
    "data:text/html;charset=utf-8,<body>test_init_default_zoom</body>";

  // Prepare the test tab
  console.log("Creating tab");
  let tab = BrowserTestUtils.addTab(gBrowser, TEST_PAGE_URL);
  let tabBrowser = gBrowser.getBrowserForTab(tab);
  await FullZoomHelper.selectTabAndWaitForLocationChange(tab);

  // 100% global zoom
  console.log("Getting default zoom");
  let defaultZoom = await FullZoomHelper.getGlobalValue();
  is(defaultZoom, 1, "Global zoom is init at 100%");
  // 100% tab zoom
  is(
    ZoomManager.getZoomForBrowser(tabBrowser),
    1,
    "Current zoom is init at 100%"
  );
  console.log("Removing tab");
  await FullZoomHelper.removeTabAndWaitForLocationChange();
});

add_task(async function test_set_default_zoom() {
  const TEST_PAGE_URL =
    "data:text/html;charset=utf-8,<body>test_set_default_zoom</body>";

  // Prepare the test tab
  console.log("Creating tab");
  let tab = BrowserTestUtils.addTab(gBrowser, TEST_PAGE_URL);
  let tabBrowser = gBrowser.getBrowserForTab(tab);
  await FullZoomHelper.selectTabAndWaitForLocationChange(tab);

  // 120% global zoom
  console.log("Changing default zoom");
  await FullZoomHelper.changeDefaultZoom(120);
  let defaultZoom = await FullZoomHelper.getGlobalValue();
  is(defaultZoom, 1.2, "Global zoom is at 120%");

  // 120% tab zoom
  await TestUtils.waitForCondition(() => {
    console.log("Current zoom is: ", ZoomManager.getZoomForBrowser(tabBrowser));
    return ZoomManager.getZoomForBrowser(tabBrowser) == 1.2;
  });
  is(
    ZoomManager.getZoomForBrowser(tabBrowser),
    1.2,
    "Current zoom matches changed default zoom"
  );
  console.log("Removing tab");
  await FullZoomHelper.removeTabAndWaitForLocationChange();
});

add_task(async function test_enlarge_reduce_reset_local_zoom() {
  const TEST_PAGE_URL =
    "data:text/html;charset=utf-8,<body>test_enlarge_reduce_reset_local_zoom</body>";

  // Prepare the test tab
  console.log("Creating tab");
  let tab = BrowserTestUtils.addTab(gBrowser, TEST_PAGE_URL);
  let tabBrowser = gBrowser.getBrowserForTab(tab);
  await FullZoomHelper.selectTabAndWaitForLocationChange(tab);

  // 120% global zoom
  console.log("Changing default zoom");
  await FullZoomHelper.changeDefaultZoom(120);
  let defaultZoom = await FullZoomHelper.getGlobalValue();
  is(defaultZoom, 1.2, "Global zoom is at 120%");

  await TestUtils.waitForCondition(() => {
    console.log(
      "Current tab zoom is ",
      ZoomManager.getZoomForBrowser(tabBrowser)
    );
    return ZoomManager.getZoomForBrowser(tabBrowser) == 1.2;
  });
  is(
    ZoomManager.getZoomForBrowser(tabBrowser),
    1.2,
    "Current tab zoom matches default zoom"
  );

  await FullZoom.enlarge();
  console.log("Enlarged!");
  defaultZoom = await FullZoomHelper.getGlobalValue();
  console.log("Current global zoom is ", defaultZoom);

  // 133% tab zoom
  await TestUtils.waitForCondition(() => {
    console.log(
      "Current tab zoom is ",
      ZoomManager.getZoomForBrowser(tabBrowser)
    );
    return ZoomManager.getZoomForBrowser(tabBrowser) == 1.33;
  });
  is(
    ZoomManager.getZoomForBrowser(tabBrowser),
    1.33,
    "Increasing zoom changes zoom of current tab."
  );
  defaultZoom = await FullZoomHelper.getGlobalValue();
  // 120% global zoom
  is(
    defaultZoom,
    1.2,
    "Increasing zoom of current tab doesn't change default zoom."
  );
  console.log("Reducing...");
  console.log(
    "Current tab zoom is ",
    ZoomManager.getZoomForBrowser(tabBrowser)
  );
  await FullZoom.reduce(); // 120% tab zoom
  console.log(
    "Current tab zoom is ",
    ZoomManager.getZoomForBrowser(tabBrowser)
  );
  await FullZoom.reduce(); // 110% tab zoom
  console.log(
    "Current tab zoom is ",
    ZoomManager.getZoomForBrowser(tabBrowser)
  );
  await FullZoom.reduce(); // 100% tab zoom
  console.log(
    "Current tab zoom is ",
    ZoomManager.getZoomForBrowser(tabBrowser)
  );

  await TestUtils.waitForCondition(() => {
    console.log(
      "Current tab zoom is ",
      ZoomManager.getZoomForBrowser(tabBrowser)
    );
    return ZoomManager.getZoomForBrowser(tabBrowser) == 1;
  });
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
  console.log("Resetting...");
  FullZoom.reset(); // 120% tab zoom
  await TestUtils.waitForCondition(() => {
    console.log(
      "Current tab zoom is ",
      ZoomManager.getZoomForBrowser(tabBrowser)
    );
    return ZoomManager.getZoomForBrowser(tabBrowser) == 1.2;
  });
  is(
    ZoomManager.getZoomForBrowser(tabBrowser),
    1.2,
    "Reseting zoom causes current tab to zoom to default zoom."
  );

  // no reset necessary, it was performed as part of the test
  console.log("Removing tab");
  await FullZoomHelper.removeTabAndWaitForLocationChange();
});
