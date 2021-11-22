"use strict";

let BASE_URL = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "http://example.com/"
);
const TEST_URL = BASE_URL + "pageopeningwindow.html";
const DUMMY_URL = BASE_URL + "bookmarklet_windowOpen_dummy.html";

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
  let keywordForBM = "openmeatab";

  let bookmarkInfo;
  let bookmarkCreated = makeBookmarkFor(
    "javascript:void open('" + TEST_URL + "')",
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
  if (!browser.currentURI || browser.currentURI.spec != TEST_URL) {
    info("Waiting for browser load");
    await BrowserTestUtils.browserLoaded(browser, false, TEST_URL);
  }
  is(
    browser.currentURI && browser.currentURI.spec,
    TEST_URL,
    "Tab with expected URL loaded."
  );
  info("Waiting to remove tab");
  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(moztab);
});
