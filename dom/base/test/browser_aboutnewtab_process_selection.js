const TEST_URL = "http://www.example.com/browser/dom/base/test/dummy.html";
const PRELOADED_STATE = "preloaded";
const CONSUMED_STATE = "consumed";

var ppmm = Services.ppmm;

add_task(async function(){
  // We want to count processes in this test, so let's disable the pre-allocated process manager.
  await SpecialPowers.pushPrefEnv({"set": [
    ["dom.ipc.processPrelaunch.enabled", false],
    ["dom.ipc.processCount", 10],
    ["dom.ipc.keepProcessesAlive.web", 10],
  ]});
});

// Ensure that the preloaded browser exists, and it's finished loading.
async function ensurePreloaded(gBrowser) {
  NewTabPagePreloading.maybeCreatePreloadedBrowser(gBrowser.ownerGlobal);
  // We cannot use the regular BrowserTestUtils helper for waiting here, since that
  // would try to insert the preloaded browser, which would only break things.
  await ContentTask.spawn(gBrowser.preloadedBrowser, null, async () => {
    await ContentTaskUtils.waitForCondition(() => {
      return content.document && content.document.readyState == "complete";
    });
  });
}

add_task(async function(){
  // This test is only relevant in e10s.
  if (!gMultiProcessBrowser)
    return;

  ppmm.releaseCachedProcesses();

  // Wait for the preloaded browser to load.
  await ensurePreloaded(gBrowser);

  // Store the number of processes (note: +1 for the parent process).
  const { childCount: originalChildCount } = ppmm;

  // Use the preloaded browser and create another one.
  BrowserOpenTab();
  let tab1 = gBrowser.selectedTab;
  await ensurePreloaded(gBrowser);

  // Check that the process count did not change.
  is(ppmm.childCount, originalChildCount, "Preloaded browser should not create a new content process.")

  // Let's do another round.
  BrowserOpenTab();
  let tab2 = gBrowser.selectedTab;
  await ensurePreloaded(gBrowser);

  // Check that the process count did not change.
  is(ppmm.childCount, originalChildCount, "Preloaded browser should (still) not create a new content process.")

  // Navigate to a content page from the parent side.
  BrowserTestUtils.loadURI(tab2.linkedBrowser, TEST_URL);
  await BrowserTestUtils.browserLoaded(tab2.linkedBrowser, false, TEST_URL);
  is(ppmm.childCount, originalChildCount + 1,
     "Navigating away from the preloaded browser (parent side) should create a new content process.")

  // Navigate to a content page from the child side.
  await BrowserTestUtils.switchTab(gBrowser, tab1);
  /* eslint-disable no-shadow */
  await ContentTask.spawn(tab1.linkedBrowser, null, async function() {
    const TEST_URL = "http://www.example.com/browser/dom/base/test/dummy.html";
    content.location.href = TEST_URL;
  });
  /* eslint-enable no-shadow */
  await BrowserTestUtils.browserLoaded(tab1.linkedBrowser, false, TEST_URL);
  is(ppmm.childCount, originalChildCount + 2,
     "Navigating away from the preloaded browser (child side) should create a new content process.")

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);

  // Make sure the preload browser does not keep any of the new processes alive.
  NewTabPagePreloading.removePreloadedBrowser(window);

  // Since we kept alive all the processes, we can shut down the ones that do
  // not host any tabs reliably.
  ppmm.releaseCachedProcesses();
});

add_task(async function preloaded_state_attribute() {
  // Wait for a preloaded browser to exist, use it, and then create another one
  await ensurePreloaded(gBrowser);
  let preloadedTabState = gBrowser.preloadedBrowser.getAttribute("preloadedState");
  is(preloadedTabState, PRELOADED_STATE, "Sanity check that the first preloaded browser has the correct attribute");

  BrowserOpenTab();
  await ensurePreloaded(gBrowser);

  // Now check that the tabs have the correct browser attributes set
  let consumedTabState = gBrowser.selectedBrowser.getAttribute("preloadedState");
  is(consumedTabState, CONSUMED_STATE, "The opened tab consumed the preloaded browser and updated the attribute");

  preloadedTabState = gBrowser.preloadedBrowser.getAttribute("preloadedState");
  is(preloadedTabState, PRELOADED_STATE, "The preloaded browser has the correct attribute");

  // Navigate away and check that the attribute has been removed altogether
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, TEST_URL);
  let navigatedTabHasState = gBrowser.selectedBrowser.hasAttribute("preloadedState");
  ok(!navigatedTabHasState, "Correctly removed the preloadState attribute when navigating away");

  // Remove tabs and preloaded browsers
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  NewTabPagePreloading.removePreloadedBrowser(window);
});
