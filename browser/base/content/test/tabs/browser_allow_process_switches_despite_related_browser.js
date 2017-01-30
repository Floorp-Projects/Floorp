/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */

const DUMMY_FILE = "dummy_page.html";

// Test for bug 1328829.
add_task(function* () {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser,
                                                        "data:text/html,Hi");
  registerCleanupFunction(function* () {
    yield BrowserTestUtils.removeTab(tab);
  });

  let promiseTab =
    BrowserTestUtils.waitForNewTab(gBrowser, "view-source:data:text/html,Hi");
  BrowserViewSource(tab.linkedBrowser);
  let viewSourceTab = yield promiseTab;
  registerCleanupFunction(function* () {
    yield BrowserTestUtils.removeTab(viewSourceTab);
  });


  let dummyPage = getChromeDir(getResolvedURI(gTestPath));
  dummyPage.append(DUMMY_FILE);
  const uriString = Services.io.newFileURI(dummyPage).spec;

  let viewSourceBrowser = viewSourceTab.linkedBrowser;
  viewSourceBrowser.loadURI(uriString);
  let href = yield BrowserTestUtils.browserLoaded(viewSourceBrowser);
  is(href, uriString,
    "Check file:// URI loads in a browser that was previously for view-source");
});
