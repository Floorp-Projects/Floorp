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
add_task(async function() {
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
add_task(async function() {
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
add_task(async function() {
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
