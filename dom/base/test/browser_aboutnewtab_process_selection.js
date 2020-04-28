const TEST_URL = "http://www.example.com/browser/dom/base/test/dummy.html";
const TEST_URL_2 = "http://example.org/browser/dom/base/test/dummy.html";
const PRELOADED_STATE = "preloaded";
const CONSUMED_STATE = "consumed";

var ppmm = Services.ppmm;

add_task(async function() {
  // We want to count processes in this test, so let's disable the pre-allocated process manager.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.ipc.processPrelaunch.enabled", false],
      ["dom.ipc.processCount", 10],
      ["dom.ipc.keepProcessesAlive.web", 10],
    ],
  });
});

add_task(async function() {
  // This test is only relevant in e10s.
  if (!gMultiProcessBrowser) {
    return;
  }

  ppmm.releaseCachedProcesses();

  // Wait for the preloaded browser to load.
  await BrowserTestUtils.maybeCreatePreloadedBrowser(gBrowser);

  // Store the number of processes.
  let expectedChildCount = ppmm.childCount;

  // Open 3 tabs using the preloaded browser.
  let tabs = [];
  for (let i = 0; i < 3; i++) {
    BrowserOpenTab();
    tabs.unshift(gBrowser.selectedTab);
    await BrowserTestUtils.maybeCreatePreloadedBrowser(gBrowser);

    // Check that the process count did not change.
    is(
      ppmm.childCount,
      expectedChildCount,
      "Preloaded browser should not create a new content process."
    );
  }

  // Navigate to a content page from the parent side.
  //
  // We should create a new content process.
  expectedChildCount += 1;
  BrowserTestUtils.loadURI(tabs[0].linkedBrowser, TEST_URL);
  await BrowserTestUtils.browserLoaded(tabs[0].linkedBrowser, false, TEST_URL);
  is(
    ppmm.childCount,
    expectedChildCount,
    "Navigating away from the preloaded browser (parent side) should create a new content process."
  );

  // Navigate to the same content page from the child side.
  //
  // We already have a content process for TEST_URL, so we don't create a new
  // one when Fission is enabled.
  expectedChildCount += gFissionBrowser ? 0 : 1;
  await BrowserTestUtils.switchTab(gBrowser, tabs[1]);
  await SpecialPowers.spawn(tabs[1].linkedBrowser, [TEST_URL], url => {
    content.location.href = url;
  });
  await BrowserTestUtils.browserLoaded(tabs[1].linkedBrowser, false, TEST_URL);
  is(
    ppmm.childCount,
    expectedChildCount,
    `Navigating away from the preloaded browser (child side, same-origin) should${
      gFissionBrowser ? " not " : " "
    }create a new content process.`
  );

  // Navigate to a new content page from the child side.
  //
  // We should create a new content process, with or without Fission.
  expectedChildCount += 1;
  await BrowserTestUtils.switchTab(gBrowser, tabs[2]);
  await ContentTask.spawn(tabs[2].linkedBrowser, TEST_URL_2, url => {
    content.location.href = url;
  });
  await BrowserTestUtils.browserLoaded(
    tabs[2].linkedBrowser,
    false,
    TEST_URL_2
  );
  is(
    ppmm.childCount,
    expectedChildCount,
    "Navigating away from the preloaded browser (child side, cross-origin) should create a new content process."
  );

  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }

  // Make sure the preload browser does not keep any of the new processes alive.
  NewTabPagePreloading.removePreloadedBrowser(window);

  // Since we kept alive all the processes, we can shut down the ones that do
  // not host any tabs reliably.
  ppmm.releaseCachedProcesses();
});

add_task(async function preloaded_state_attribute() {
  // Wait for a preloaded browser to exist, use it, and then create another one
  await BrowserTestUtils.maybeCreatePreloadedBrowser(gBrowser);
  let preloadedTabState = gBrowser.preloadedBrowser.getAttribute(
    "preloadedState"
  );
  is(
    preloadedTabState,
    PRELOADED_STATE,
    "Sanity check that the first preloaded browser has the correct attribute"
  );

  BrowserOpenTab();
  await BrowserTestUtils.maybeCreatePreloadedBrowser(gBrowser);

  // Now check that the tabs have the correct browser attributes set
  let consumedTabState = gBrowser.selectedBrowser.getAttribute(
    "preloadedState"
  );
  is(
    consumedTabState,
    CONSUMED_STATE,
    "The opened tab consumed the preloaded browser and updated the attribute"
  );

  preloadedTabState = gBrowser.preloadedBrowser.getAttribute("preloadedState");
  is(
    preloadedTabState,
    PRELOADED_STATE,
    "The preloaded browser has the correct attribute"
  );

  // Navigate away and check that the attribute has been removed altogether
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, TEST_URL);
  await BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    TEST_URL
  );
  let navigatedTabHasState = gBrowser.selectedBrowser.hasAttribute(
    "preloadedState"
  );
  ok(
    !navigatedTabHasState,
    "Correctly removed the preloadState attribute when navigating away"
  );

  // Remove tabs and preloaded browsers
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  NewTabPagePreloading.removePreloadedBrowser(window);
});
