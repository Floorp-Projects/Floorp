/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */

const DUMMY_FILE = "dummy_page.html";
const DATA_URI = "data:text/html,Hi";
const DATA_URI_SOURCE = "view-source:" + DATA_URI;

// Test for bug 1328829.
add_task(async function () {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, DATA_URI);
  registerCleanupFunction(async function () {
    BrowserTestUtils.removeTab(tab);
  });

  let promiseTab = BrowserTestUtils.waitForNewTab(gBrowser, DATA_URI_SOURCE);
  BrowserViewSource(tab.linkedBrowser);
  let viewSourceTab = await promiseTab;
  registerCleanupFunction(async function () {
    BrowserTestUtils.removeTab(viewSourceTab);
  });

  let dummyPage = getChromeDir(getResolvedURI(gTestPath));
  dummyPage.append(DUMMY_FILE);
  dummyPage.normalize();
  const uriString = Services.io.newFileURI(dummyPage).spec;

  let viewSourceBrowser = viewSourceTab.linkedBrowser;
  let promiseLoad = BrowserTestUtils.browserLoaded(
    viewSourceBrowser,
    false,
    uriString
  );
  BrowserTestUtils.startLoadingURIString(viewSourceBrowser, uriString);
  let href = await promiseLoad;
  is(
    href,
    uriString,
    "Check file:// URI loads in a browser that was previously for view-source"
  );
});
