const TEST_URL = "http://www.example.com/browser/dom/base/test/dummy.html";

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
  gBrowser._createPreloadBrowser();
  // We cannot use the regular BrowserTestUtils helper for waiting here, since that
  // would try to insert the preloaded browser, which would only break things.
  await BrowserTestUtils.waitForCondition( () => {
    return gBrowser._preloadedBrowser.contentDocument.readyState == "complete";
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
  tab2.linkedBrowser.loadURI(TEST_URL);
  await BrowserTestUtils.browserLoaded(tab2.linkedBrowser, false, TEST_URL);
  is(ppmm.childCount, originalChildCount + 1,
     "Navigating away from the preloaded browser (parent side) should create a new content process.")

  // Navigate to a content page from the child side.
  await BrowserTestUtils.switchTab(gBrowser, tab1);
  await ContentTask.spawn(tab1.linkedBrowser, null, async function() {
    const TEST_URL = "http://www.example.com/browser/dom/base/test/dummy.html";
    content.location.href = TEST_URL;
  });
  await BrowserTestUtils.browserLoaded(tab1.linkedBrowser, false, TEST_URL);
  is(ppmm.childCount, originalChildCount + 2,
     "Navigating away from the preloaded browser (child side) should create a new content process.")

  await BrowserTestUtils.removeTab(tab1);
  await BrowserTestUtils.removeTab(tab2);

  // Make sure the preload browser does not keep any of the new processes alive.
  gBrowser.removePreloadedBrowser();

  // Since we kept alive all the processes, we can shut down the ones that do
  // not host any tabs reliably.
  ppmm.releaseCachedProcesses();
});
