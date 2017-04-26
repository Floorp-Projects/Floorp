"use strict";

let Cu = Components.utils;
Cu.import("resource://gre/modules/Services.jsm");

/**
 * Tests that opening a new tab opens a page with the expected activity stream
 * content.
 *
 * XXX /browser/components/newtab/tests/browser/browser_newtab_overrides in
 * mozilla-central is where this test was adapted from.  Once we get decide on
 * and implement how we're going to set the URL in mozilla-central, we may well
 * want to (separately from this test), clone/adapt that entire file for our
 * new setup.
 */
add_task(async function checkActivityStreamLoads() {
  const asURL = "resource://activity-stream/data/content/activity-stream.html";

  // simulate a newtab open as a user would
  BrowserOpenTab();

  // wait until the browser loads
  let browser = gBrowser.selectedBrowser;
  await BrowserTestUtils.browserLoaded(browser);

  // check what the content task thinks has been loaded.
  await ContentTask.spawn(browser, {url: asURL}, args => {
    Assert.ok(content.document.querySelector("body.activity-stream"),
      'Got <body class="activity-stream" Element');
  });

  // avoid leakage
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
