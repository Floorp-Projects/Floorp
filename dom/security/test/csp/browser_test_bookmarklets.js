"use strict";

let BASE_URL = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "http://example.com/"
);
const DUMMY_URL = BASE_URL + "file_test_browser_bookmarklets.html";

function makeBookmarkFor(url, keyword) {
  return Promise.all([
    PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      title: "bookmarklet",
      url,
    }),
    PlacesUtils.keywords.insert({ url, keyword }),
  ]);
}
/* Test Description:
 * 1 - Load a Page with CSP script-src: none
 * 2 - Create a bookmarklet with javascript:window.open('about:blank')
 * 3 - Select and enter the bookmarklet
 *  A new tab with about:blank should be opened
 */
add_task(async function openKeywordBookmarkWithWindowOpen() {
  // This is the current default, but let's not assume that...
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.link.open_newwindow", 3],
      ["dom.disable_open_during_load", true],
    ],
  });

  let moztab;
  let tabOpened = BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    DUMMY_URL
  ).then(tab => {
    moztab = tab;
  });
  let keywordForBM = "openNewWindowBookmarklet";

  let bookmarkInfo;
  let bookmarkCreated = makeBookmarkFor(
    `javascript: window.open("about:blank")`,
    keywordForBM
  ).then(values => {
    bookmarkInfo = values[0];
  });
  await Promise.all([tabOpened, bookmarkCreated]);

  registerCleanupFunction(function() {
    return Promise.all([
      PlacesUtils.bookmarks.remove(bookmarkInfo),
      PlacesUtils.keywords.remove(keywordForBM),
    ]);
  });
  gURLBar.value = keywordForBM;
  gURLBar.focus();

  let tabCreatedPromise = BrowserTestUtils.waitForEvent(
    gBrowser.tabContainer,
    "TabOpen"
  );
  EventUtils.synthesizeKey("KEY_Enter");
  info("Waiting for tab being created");
  let { target: tab } = await tabCreatedPromise;
  info("Got tab");
  let browser = tab.linkedBrowser;
  if (!browser.currentURI || browser.currentURI.spec != "about:blank") {
    info("Waiting for browser load");
    await BrowserTestUtils.browserLoaded(browser, false, "about:blank");
  }
  is(
    browser.currentURI && browser.currentURI.spec,
    "about:blank",
    "Tab with expected URL loaded."
  );
  info("Waiting to remove tab");
  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(moztab);
});
