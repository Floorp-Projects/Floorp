const kURL = "http://example.org/browser/browser/base/content/test/urlbar/dummy_page.html";

function* addBookmark(bookmark) {
  if (bookmark.keyword) {
    yield PlacesUtils.keywords.insert({
      keyword: bookmark.keyword,
      url: bookmark.url,
    });
  }

  let bm = yield PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: bookmark.url,
    title: bookmark.title,
  });

  registerCleanupFunction(function* () {
    yield PlacesUtils.bookmarks.remove(bm);
    if (bookmark.keyword) {
      yield PlacesUtils.keywords.remove(bookmark.keyword);
    }
  });
}

/**
 * Check that if the user hits enter and ctrl-t at the same time, we open the URL in the right tab.
 */
add_task(function* hitEnterLoadInRightTab() {
  info("Opening new tab");
  let oldTabCreatedPromise = BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "TabOpen");
  BrowserOpenTab();
  let oldTab = (yield oldTabCreatedPromise).target;
  let oldTabLoadedPromise = BrowserTestUtils.browserLoaded(oldTab.linkedBrowser, false, kURL);
  oldTabLoadedPromise.then(() => info("Old tab loaded"));
  let newTabCreatedPromise = BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "TabOpen");

  info("Creating bookmark and keyword");
  yield addBookmark({title: "Test for keyword bookmark and URL", url: kURL, keyword: "urlbarkeyword"});
  info("Filling URL bar, sending <return> and opening a tab");
  gURLBar.value = "urlbarkeyword";
  gURLBar.select();
  EventUtils.sendKey("return");
  BrowserOpenTab();
  info("Waiting for new tab");
  let newTab = (yield newTabCreatedPromise).target;
  info("Created new tab; waiting for either tab to load");
  let newTabLoadedPromise = BrowserTestUtils.browserLoaded(newTab.linkedBrowser, false, kURL);
  newTabLoadedPromise.then(() => info("New tab loaded"));
  yield Promise.race([newTabLoadedPromise, oldTabLoadedPromise]);
  is(newTab.linkedBrowser.currentURI.spec, "about:newtab", "New tab still has about:newtab");
  is(oldTab.linkedBrowser.currentURI.spec, kURL, "Old tab loaded URL");
  info("Closing new tab");
  yield BrowserTestUtils.removeTab(newTab);
  info("Closing old tab");
  yield BrowserTestUtils.removeTab(oldTab);
  info("Finished");
});
