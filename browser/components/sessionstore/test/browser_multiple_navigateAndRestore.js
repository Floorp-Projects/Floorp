"use strict";

const PAGE_1 = "data:text/html,<html><body>A%20regular,%20everyday,%20normal%20page.";
const PAGE_2 = "data:text/html,<html><body>Another%20regular,%20everyday,%20normal%20page.";

add_task(function*() {
  // Load an empty, non-remote tab at about:blank...
  let tab = gBrowser.addTab("about:blank", {
    forceNotRemote: true,
  });
  gBrowser.selectedTab = tab;
  let browser = gBrowser.selectedBrowser;
  ok(!browser.isRemoteBrowser, "Ensure browser is not remote");
  // Load a remote page, and then another remote page immediately
  // after.
  browser.loadURI(PAGE_1);
  browser.stop();
  browser.loadURI(PAGE_2);
  yield BrowserTestUtils.browserLoaded(browser);

  ok(browser.isRemoteBrowser, "Should have switched remoteness");
  yield TabStateFlusher.flush(browser);
  let state = JSON.parse(ss.getTabState(tab));
  let entries = state.entries;
  is(entries.length, 1, "There should only be one entry");
  is(entries[0].url, PAGE_2, "Should have PAGE_2 as the sole history entry");
  is(browser.currentURI.spec, PAGE_2, "Should have PAGE_2 as the browser currentURI");

  yield ContentTask.spawn(browser, PAGE_2, function*(PAGE_2) {
    docShell.QueryInterface(Ci.nsIWebNavigation);
    is(docShell.currentURI.spec, PAGE_2,
       "Content should have PAGE_2 as the browser currentURI");
  });

  yield BrowserTestUtils.removeTab(tab);
});
