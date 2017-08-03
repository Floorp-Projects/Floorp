"use strict";

/**
 * Helper to test that a newtab page loads its html document.
 *
 * @param selector {String} CSS selector to find an element in newtab content
 * @param message {String} Description of the test printed with the assertion
 */
async function checkNewtabLoads(selector, message) {
  // simulate a newtab open as a user would
  BrowserOpenTab();

  // wait until the browser loads
  let browser = gBrowser.selectedBrowser;
  await waitForPreloaded(browser);

  // check what the content task thinks has been loaded.
  let found = await ContentTask.spawn(browser, selector, arg =>
    content.document.querySelector(arg) !== null);
  ok(found, message);

  // avoid leakage
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
}

// Test with activity stream on
async function checkActivityStreamLoads() {
  await checkNewtabLoads("body.activity-stream", "Got <body class='activity-stream'> Element");
}

// Run a first time not from a preloaded browser
add_task(async function checkActivityStreamNotPreloadedLoad() {
  gBrowser.removePreloadedBrowser();
  await checkActivityStreamLoads();
});

// Run a second time from a preloaded browser
add_task(checkActivityStreamLoads);

// Test with activity stream off
async function checkNotActivityStreamLoads() {
  await SpecialPowers.pushPrefEnv({set: [["browser.newtabpage.activity-stream.enabled", false]]});
  await checkNewtabLoads("body:not(.activity-stream)", "Got <body> Element not for activity-stream");
}

// Run a first time not from a preloaded browser
add_task(async function checkNotActivityStreamNotPreloadedLoad() {
  gBrowser.removePreloadedBrowser();
  await checkNotActivityStreamLoads();
});

// Run a second time from a preloaded browser
add_task(checkNotActivityStreamLoads);
