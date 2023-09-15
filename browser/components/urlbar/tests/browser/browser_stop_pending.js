/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

const SLOW_PAGE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "http://www.example.com"
  ) + "slow-page.sjs";
const SLOW_PAGE2 =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "http://mochi.test:8888"
  ) + "slow-page.sjs?faster";

/**
 * Check that if we:
 * 1) have a loaded page
 * 2) load a separate URL
 * 3) before the URL for step 2 has finished loading, load a third URL
 * we don't revert to the URL from (1).
 */
add_task(async function () {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com",
    true,
    true
  );

  let initialValue = gURLBar.untrimmedValue;
  let expectedURLBarChange = SLOW_PAGE;
  let sawChange = false;
  let handler = () => {
    isnot(
      gURLBar.untrimmedValue,
      initialValue,
      "Should not revert URL bar value!"
    );
    if (gURLBar.getAttribute("pageproxystate") == "valid") {
      sawChange = true;
      is(
        gURLBar.untrimmedValue,
        expectedURLBarChange,
        "Should set expected URL bar value!"
      );
    }
  };

  let obs = new MutationObserver(handler);

  obs.observe(gURLBar.textbox, { attributes: true });
  gURLBar.value = SLOW_PAGE;
  gURLBar.handleCommand();

  // If this ever starts going intermittent, we've broken this.
  await new Promise(resolve => setTimeout(resolve, 200));
  expectedURLBarChange = SLOW_PAGE2;
  let pageLoadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  gURLBar.value = expectedURLBarChange;
  gURLBar.handleCommand();
  is(
    gURLBar.untrimmedValue,
    expectedURLBarChange,
    "Should not have changed URL bar value synchronously."
  );
  await pageLoadPromise;
  ok(
    sawChange,
    "The URL bar change handler should have been called by the time the page was loaded"
  );
  obs.disconnect();
  obs = null;
  BrowserTestUtils.removeTab(tab);
});

/**
 * Check that if we:
 * 1) middle-click a link to a separate page whose server doesn't respond
 * 2) we switch to that tab and stop the request
 *
 * The URL bar continues to contain the URL of the page we wanted to visit.
 */
add_task(async function () {
  let socket = Cc["@mozilla.org/network/server-socket;1"].createInstance(
    Ci.nsIServerSocket
  );
  socket.init(-1, true, -1);
  const PORT = socket.port;
  registerCleanupFunction(() => {
    socket.close();
  });

  const BASE_PAGE = TEST_BASE_URL + "dummy_page.html";
  const SLOW_HOST = `https://localhost:${PORT}/`;
  info("Using URLs: " + SLOW_HOST);
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, BASE_PAGE);
  info("opened tab");
  await SpecialPowers.spawn(tab.linkedBrowser, [SLOW_HOST], URL => {
    let link = content.document.createElement("a");
    link.href = URL;
    link.textContent = "click me to open a slow page";
    link.id = "clickme";
    content.document.body.appendChild(link);
  });
  info("added link");
  let newTabPromise = BrowserTestUtils.waitForEvent(
    gBrowser.tabContainer,
    "TabOpen"
  );
  // Middle click the link:
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#clickme",
    { button: 1 },
    tab.linkedBrowser
  );
  // get new tab, switch to it
  let newTab = (await newTabPromise).target;
  await BrowserTestUtils.switchTab(gBrowser, newTab);
  is(gURLBar.untrimmedValue, SLOW_HOST, "Should have slow page in URL bar");
  let browserStoppedPromise = BrowserTestUtils.browserStopped(
    newTab.linkedBrowser,
    null,
    true
  );
  BrowserStop();
  await browserStoppedPromise;

  is(
    gURLBar.untrimmedValue,
    SLOW_HOST,
    "Should still have slow page in URL bar after stop"
  );
  BrowserTestUtils.removeTab(newTab);
  BrowserTestUtils.removeTab(tab);
});
/**
 * Check that if we:
 * 1) middle-click a link to a separate page whose server doesn't respond
 * 2) we alter the URL on that page to some other server that doesn't respond
 * 3) we stop the request
 *
 * The URL bar continues to contain the second URL.
 */
add_task(async function () {
  let socket = Cc["@mozilla.org/network/server-socket;1"].createInstance(
    Ci.nsIServerSocket
  );
  socket.init(-1, true, -1);
  const PORT1 = socket.port;
  let socket2 = Cc["@mozilla.org/network/server-socket;1"].createInstance(
    Ci.nsIServerSocket
  );
  socket2.init(-1, true, -1);
  const PORT2 = socket2.port;
  registerCleanupFunction(() => {
    socket.close();
    socket2.close();
  });

  const BASE_PAGE = TEST_BASE_URL + "dummy_page.html";
  const SLOW_HOST1 = `https://localhost:${PORT1}/`;
  const SLOW_HOST2 = `https://localhost:${PORT2}/`;
  info("Using URLs: " + SLOW_HOST1 + " and " + SLOW_HOST2);
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, BASE_PAGE);
  info("opened tab");
  await SpecialPowers.spawn(tab.linkedBrowser, [SLOW_HOST1], URL => {
    let link = content.document.createElement("a");
    link.href = URL;
    link.textContent = "click me to open a slow page";
    link.id = "clickme";
    content.document.body.appendChild(link);
  });
  info("added link");
  let newTabPromise = BrowserTestUtils.waitForEvent(
    gBrowser.tabContainer,
    "TabOpen"
  );
  // Middle click the link:
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#clickme",
    { button: 1 },
    tab.linkedBrowser
  );
  // get new tab, switch to it
  let newTab = (await newTabPromise).target;
  await BrowserTestUtils.switchTab(gBrowser, newTab);
  is(gURLBar.untrimmedValue, SLOW_HOST1, "Should have slow page in URL bar");
  let browserStoppedPromise = BrowserTestUtils.browserStopped(
    newTab.linkedBrowser,
    null,
    true
  );
  gURLBar.value = SLOW_HOST2;
  gURLBar.handleCommand();
  await browserStoppedPromise;

  is(
    gURLBar.untrimmedValue,
    SLOW_HOST2,
    "Should have second slow page in URL bar"
  );
  browserStoppedPromise = BrowserTestUtils.browserStopped(
    newTab.linkedBrowser,
    null,
    true
  );
  BrowserStop();
  await browserStoppedPromise;

  is(
    gURLBar.untrimmedValue,
    SLOW_HOST2,
    "Should still have second slow page in URL bar after stop"
  );
  BrowserTestUtils.removeTab(newTab);
  BrowserTestUtils.removeTab(tab);
});

/**
 * 1) Try to load page 0 and wait for it to finish loading.
 * 2) Try to load page 1 and wait for it to finish loading.
 * 3) Try to load SLOW_PAGE, and then before it finishes loading, navigate back.
 *    - We should be taken to page 0.
 */
add_task(async function testCorrectUrlBarAfterGoingBackDuringAnotherLoad() {
  // Load example.org
  let page0 = "http://example.org/";
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    page0,
    true,
    true
  );

  // Load example.com in the same browser
  let page1 = "http://example.com/";
  let loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, page1);
  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, page1);
  await loaded;

  let initialValue = gURLBar.untrimmedValue;
  let expectedURLBarChange = SLOW_PAGE;
  let sawChange = false;
  let goneBack = false;
  let handler = () => {
    if (!goneBack) {
      isnot(
        gURLBar.untrimmedValue,
        initialValue,
        `Should not revert URL bar value to ${initialValue}`
      );
    }

    if (gURLBar.getAttribute("pageproxystate") == "valid") {
      sawChange = true;
      is(
        gURLBar.untrimmedValue,
        expectedURLBarChange,
        `Should set expected URL bar value - ${expectedURLBarChange}`
      );
    }
  };

  let obs = new MutationObserver(handler);

  obs.observe(gURLBar.textbox, { attributes: true });
  // Set the value of url bar to SLOW_PAGE
  gURLBar.value = SLOW_PAGE;
  gURLBar.handleCommand();

  // Copied from the first test case:
  // If this ever starts going intermittent, we've broken this.
  await new Promise(resolve => setTimeout(resolve, 200));

  expectedURLBarChange = page0;
  let pageLoadPromise = BrowserTestUtils.browserStopped(
    tab.linkedBrowser,
    page0
  );

  // Wait until we can go back
  await TestUtils.waitForCondition(() => tab.linkedBrowser.canGoBack);
  ok(tab.linkedBrowser.canGoBack, "can go back");

  // Navigate back from SLOW_PAGE. We should be taken to page 0 now.
  tab.linkedBrowser.goBack();
  goneBack = true;
  is(
    gURLBar.untrimmedValue,
    SLOW_PAGE,
    "Should not have changed URL bar value synchronously."
  );
  // Wait until page 0 have finished loading.
  await pageLoadPromise;
  is(
    gURLBar.untrimmedValue,
    page0,
    "Should not have changed URL bar value synchronously."
  );
  ok(
    sawChange,
    "The URL bar change handler should have been called by the time the page was loaded"
  );
  obs.disconnect();
  obs = null;
  BrowserTestUtils.removeTab(tab);
});

/**
 * 1) Try to load page 1 and wait for it to finish loading.
 * 2) Start loading SLOW_PAGE (it won't finish loading)
 * 3) Reload the page. We should have loaded page 1 now.
 */
add_task(async function testCorrectUrlBarAfterReloadingDuringSlowPageLoad() {
  // Load page 1 - example.com
  let page1 = "http://example.com/";
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    page1,
    true,
    true
  );

  let initialValue = gURLBar.untrimmedValue;
  let expectedURLBarChange = SLOW_PAGE;
  let sawChange = false;
  let hasReloaded = false;
  let handler = () => {
    if (!hasReloaded) {
      isnot(
        gURLBar.untrimmedValue,
        initialValue,
        "Should not revert URL bar value!"
      );
    }
    if (gURLBar.getAttribute("pageproxystate") == "valid") {
      sawChange = true;
      is(
        gURLBar.untrimmedValue,
        expectedURLBarChange,
        "Should set expected URL bar value!"
      );
    }
  };

  let obs = new MutationObserver(handler);

  obs.observe(gURLBar.textbox, { attributes: true });
  // Start loading SLOW_PAGE
  gURLBar.value = SLOW_PAGE;
  gURLBar.handleCommand();

  // Copied from the first test: If this ever starts going intermittent,
  // we've broken this.
  await new Promise(resolve => setTimeout(resolve, 200));

  expectedURLBarChange = page1;
  let pageLoadPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false,
    page1
  );
  // Reload the page
  tab.linkedBrowser.reload();
  hasReloaded = true;
  is(
    gURLBar.untrimmedValue,
    SLOW_PAGE,
    "Should not have changed URL bar value synchronously."
  );
  // Wait for page1 to be loaded due to a reload while the slow page was still loading
  await pageLoadPromise;
  ok(
    sawChange,
    "The URL bar change handler should have been called by the time the page was loaded"
  );
  obs.disconnect();
  obs = null;
  BrowserTestUtils.removeTab(tab);
});

/**
 * 1) Try to load example.com and wait for it to finish loading.
 * 2) Start loading SLOW_PAGE and then stop the load before the load completes
 * 3) Check that example.com has been loaded as a result of stopping SLOW_PAGE
 *    load.
 */
add_task(async function testCorrectUrlBarAfterStoppingTheLoad() {
  // Load page 1
  let page1 = "http://example.com/";
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    page1,
    true,
    true
  );

  let initialValue = gURLBar.untrimmedValue;
  let expectedURLBarChange = SLOW_PAGE;
  let sawChange = false;
  let hasStopped = false;
  let handler = () => {
    if (!hasStopped) {
      isnot(
        gURLBar.untrimmedValue,
        initialValue,
        "Should not revert URL bar value!"
      );
    }
    if (gURLBar.getAttribute("pageproxystate") == "valid") {
      sawChange = true;
      is(
        gURLBar.untrimmedValue,
        expectedURLBarChange,
        "Should set expected URL bar value!"
      );
    }
  };

  let obs = new MutationObserver(handler);

  obs.observe(gURLBar.textbox, { attributes: true });
  // Start loading SLOW_PAGE
  gURLBar.value = SLOW_PAGE;
  gURLBar.handleCommand();

  // Copied from the first test case:
  // If this ever starts going intermittent, we've broken this.
  await new Promise(resolve => setTimeout(resolve, 200));

  // We expect page 1 to be loaded after the SLOW_PAGE load is stopped.
  expectedURLBarChange = page1;
  let pageLoadPromise = BrowserTestUtils.browserStopped(
    tab.linkedBrowser,
    SLOW_PAGE,
    true
  );
  // Stop the SLOW_PAGE load
  tab.linkedBrowser.stop();
  hasStopped = true;
  is(
    gURLBar.untrimmedValue,
    SLOW_PAGE,
    "Should not have changed URL bar value synchronously."
  );
  // Wait for SLOW_PAGE load to stop
  await pageLoadPromise;

  ok(
    sawChange,
    "The URL bar change handler should have been called by the time the page was loaded"
  );
  obs.disconnect();
  obs = null;
  BrowserTestUtils.removeTab(tab);
});
