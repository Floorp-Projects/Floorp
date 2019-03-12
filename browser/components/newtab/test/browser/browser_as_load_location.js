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
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
}

// Test with activity stream on
async function checkActivityStreamLoads() {
  await checkNewtabLoads("body.activity-stream", "Got <body class='activity-stream'> Element");
}

// Run a first time not from a preloaded browser
add_task(async function checkActivityStreamNotPreloadedLoad() {
  NewTabPagePreloading.removePreloadedBrowser(window);
  await checkActivityStreamLoads();
});

// Run a second time from a preloaded browser
add_task(checkActivityStreamLoads);
