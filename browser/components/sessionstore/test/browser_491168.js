"use strict";

const REFERRER1 = "http://example.org/?" + Date.now();
const REFERRER2 = "http://example.org/?" + Math.random();

add_task(function* () {
  function* checkDocumentReferrer(referrer, msg) {
    yield ContentTask.spawn(gBrowser.selectedBrowser, { referrer, msg }, function* (args) {
      Assert.equal(content.document.referrer, args.referrer, args.msg);
    });
  }

  // Add a new tab.
  let tab = gBrowser.selectedTab = gBrowser.addTab("about:blank");
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Load a new URI with a specific referrer.
  let referrerURI = Services.io.newURI(REFERRER1, null, null);
  browser.loadURI("http://example.org", referrerURI, null);
  yield promiseBrowserLoaded(browser);

  yield TabStateFlusher.flush(browser);
  let tabState = JSON.parse(ss.getTabState(tab));
  is(tabState.entries[0].referrer,  REFERRER1,
     "Referrer retrieved via getTabState matches referrer set via loadURI.");

  tabState.entries[0].referrer = REFERRER2;
  yield promiseTabState(tab, tabState);

  yield checkDocumentReferrer(REFERRER2,
    "document.referrer matches referrer set via setTabState.");
  gBrowser.removeCurrentTab();

  // Restore the closed tab.
  tab = ss.undoCloseTab(window, 0);
  yield promiseTabRestored(tab);

  yield checkDocumentReferrer(REFERRER2,
    "document.referrer is still correct after closing and reopening the tab.");
  gBrowser.removeCurrentTab();
});
