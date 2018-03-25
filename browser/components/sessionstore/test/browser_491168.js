"use strict";

const REFERRER1 = "http://example.org/?" + Date.now();
const REFERRER2 = "http://example.org/?" + Math.random();

add_task(async function() {
  async function checkDocumentReferrer(referrer, msg) {
    await ContentTask.spawn(gBrowser.selectedBrowser, { referrer, msg }, async function(args) {
      Assert.equal(content.document.referrer, args.referrer, args.msg);
    });
  }

  // Add a new tab.
  let tab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  let browser = tab.linkedBrowser;
  await promiseBrowserLoaded(browser);

  // Load a new URI with a specific referrer.
  let referrerURI = Services.io.newURI(REFERRER1);
  browser.loadURI("http://example.org", referrerURI, null);
  await promiseBrowserLoaded(browser);

  await TabStateFlusher.flush(browser);
  let tabState = JSON.parse(ss.getTabState(tab));
  is(tabState.entries[0].referrer, REFERRER1,
     "Referrer retrieved via getTabState matches referrer set via loadURI.");

  tabState.entries[0].referrer = REFERRER2;
  await promiseTabState(tab, tabState);

  await checkDocumentReferrer(REFERRER2,
    "document.referrer matches referrer set via setTabState.");
  gBrowser.removeCurrentTab();

  // Restore the closed tab.
  tab = ss.undoCloseTab(window, 0);
  await promiseTabRestored(tab);

  await checkDocumentReferrer(REFERRER2,
    "document.referrer is still correct after closing and reopening the tab.");
  gBrowser.removeCurrentTab();
});
