/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_sitespecific_iframe_global_zoom() {
  const TEST_PAGE_URL =
    'data:text/html;charset=utf-8,<body>test_sitespecific_iframe_global_zoom<iframe src=""></iframe></body>';
  const TEST_IFRAME_URL = "https://example.com/";

  // Prepare the test tab
  let tab = BrowserTestUtils.addTab(gBrowser);
  let tabBrowser = gBrowser.getBrowserForTab(tab);
  await FullZoomHelper.selectTabAndWaitForLocationChange(tab);
  await FullZoomHelper.load(tab, TEST_PAGE_URL);

  // 67% global zoom
  await FullZoomHelper.changeDefaultZoom(67);
  let defaultZoom = await FullZoomHelper.getGlobalValue();
  is(defaultZoom, 0.67, "Global zoom is at 67%");
  await TestUtils.waitForCondition(
    () => ZoomManager.getZoomForBrowser(tabBrowser) == 0.67
  );

  let zoomLevel = ZoomManager.getZoomForBrowser(tabBrowser);
  is(zoomLevel, 0.67, "tab zoom has been set to 67%");

  let frameLoadedPromise = BrowserTestUtils.browserLoaded(
    tabBrowser,
    true,
    TEST_IFRAME_URL
  );
  SpecialPowers.spawn(tabBrowser, [TEST_IFRAME_URL], url => {
    content.document.querySelector("iframe").src = url;
  });
  let loadedURL = await frameLoadedPromise;
  is(loadedURL, TEST_IFRAME_URL, "got the load event for the iframe");

  let frameZoom = await SpecialPowers.spawn(
    gBrowser.selectedBrowser.browsingContext.getChildren()[0],
    [],
    async () => {
      await ContentTaskUtils.waitForCondition(() => {
        return content.docShell.contentViewer.fullZoom.toFixed(2) == 0.67;
      });
      return content.docShell.contentViewer.fullZoom.toFixed(2);
    }
  );

  is(frameZoom, zoomLevel, "global zoom is reflected in iframe");

  await FullZoomHelper.removeTabAndWaitForLocationChange();
});

add_task(async function test_sitespecific_global_zoom_enlarge() {
  const TEST_PAGE_URL =
    'data:text/html;charset=utf-8,<body>test_sitespecific_global_zoom_enlarge<iframe src=""></iframe></body>';
  const TEST_IFRAME_URL = "https://example.org/";

  // Prepare the test tab
  let tab = BrowserTestUtils.addTab(gBrowser);
  let tabBrowser = gBrowser.getBrowserForTab(tab);
  await FullZoomHelper.selectTabAndWaitForLocationChange(tab);
  await FullZoomHelper.load(tab, TEST_PAGE_URL);

  // 67% global zoom persists from previous test

  let frameLoadedPromise = BrowserTestUtils.browserLoaded(
    tabBrowser,
    true,
    TEST_IFRAME_URL
  );
  SpecialPowers.spawn(tabBrowser, [TEST_IFRAME_URL], url => {
    content.document.querySelector("iframe").src = url;
  });
  let loadedURL = await frameLoadedPromise;
  is(loadedURL, TEST_IFRAME_URL, "got the load event for the iframe");
  await FullZoom.enlarge();
  // 80% local zoom
  await TestUtils.waitForCondition(() => {
    return ZoomManager.getZoomForBrowser(tabBrowser) == 0.8;
  });
  is(ZoomManager.getZoomForBrowser(tabBrowser), 0.8, "Local zoom is increased");

  let frameZoom = await SpecialPowers.spawn(
    gBrowser.selectedBrowser.browsingContext.getChildren()[0],
    [],
    async () => {
      await ContentTaskUtils.waitForCondition(() => {
        return content.docShell.contentViewer.fullZoom.toFixed(2) == 0.8;
      });
      return content.docShell.contentViewer.fullZoom.toFixed(2);
    }
  );

  is(frameZoom, 0.8, "(without fission) iframe zoom matches page zoom");

  // To ensure site-specific zoom settings don't persist
  // across multiple runs, we reset the tab and iframe after use.
  await FullZoom.reduce();
  await FullZoomHelper.removeTabAndWaitForLocationChange();
});
