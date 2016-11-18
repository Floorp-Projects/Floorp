"use strict";

const TEST_URL = 'http://example.com/browser/browser/components/places/tests/browser/pageopeningwindow.html';

function makeBookmarkFor(url, keyword) {
  return Promise.all([
    PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                   title: "bookmarklet",
                                   url: url }),
    PlacesUtils.keywords.insert({url: url,
                                 keyword: keyword})
  ]);

}

add_task(function* openKeywordBookmarkWithWindowOpen() {
  // This is the current default, but let's not assume that...
  yield SpecialPowers.pushPrefEnv({"set": [
    [ 'browser.link.open_newwindow', 3 ],
    [ 'dom.disable_open_during_load', true ]
  ]});

  let moztab;
  let tabOpened = BrowserTestUtils.openNewForegroundTab(gBrowser, "about:mozilla")
                    .then((tab) => { moztab = tab; });
  let keywordForBM = "openmeatab";

  let bookmarkInfo;
  let bookmarkCreated =
    makeBookmarkFor("javascript:void open('" + TEST_URL + "')", keywordForBM)
        .then((values) => {
          bookmarkInfo = values[0];
        });
  yield Promise.all([tabOpened, bookmarkCreated]);

  registerCleanupFunction(function() {
    return Promise.all([
      PlacesUtils.bookmarks.remove(bookmarkInfo),
      PlacesUtils.keywords.remove(keywordForBM)
    ]);
  });
  gURLBar.value = keywordForBM;
  gURLBar.focus();

  let tabCreatedPromise = BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "TabOpen");
  EventUtils.synthesizeKey("VK_RETURN", {});

  info("Waiting for tab being created");
  let {target: tab} = yield tabCreatedPromise;
  info("Got tab");
  let browser = tab.linkedBrowser;
  if (!browser.currentURI || browser.currentURI.spec != TEST_URL) {
    info("Waiting for browser load");
    yield BrowserTestUtils.browserLoaded(browser);
  }
  is(browser.currentURI && browser.currentURI.spec, TEST_URL, "Tab with expected URL loaded.");
  info("Waiting to remove tab");
  yield Promise.all([ BrowserTestUtils.removeTab(tab),
                      BrowserTestUtils.removeTab(moztab) ]);
});
