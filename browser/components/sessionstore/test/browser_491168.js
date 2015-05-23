"use strict";

const REFERRER1 = "http://example.org/?" + Date.now();
const REFERRER2 = "http://example.org/?" + Math.random();

add_task(function* () {
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

  is((yield promiseDocumentReferrer()), REFERRER2,
     "document.referrer matches referrer set via setTabState.");
  gBrowser.removeCurrentTab();

  // Restore the closed tab.
  tab = ss.undoCloseTab(window, 0);
  yield promiseTabRestored(tab);

  is((yield promiseDocumentReferrer()), REFERRER2,
     "document.referrer is still correct after closing and reopening the tab.");
  gBrowser.removeCurrentTab();
});

function promiseDocumentReferrer() {
  return ContentTask.spawn(gBrowser.selectedBrowser, null, function* () {
    return content.document.referrer;
  });
}
