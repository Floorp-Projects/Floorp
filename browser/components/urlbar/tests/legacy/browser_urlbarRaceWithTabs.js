const TEST_PATH = getRootDirectory(gTestPath)
  .replace("chrome://mochitests/content", "http://example.com");
const TEST_URL = `${TEST_PATH}dummy_page.html`;

async function addBookmark(bookmark) {
  if (bookmark.keyword) {
    await PlacesUtils.keywords.insert({
      keyword: bookmark.keyword,
      url: bookmark.url,
    });
  }

  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: bookmark.url,
    title: bookmark.title,
  });

  registerCleanupFunction(async function() {
    await PlacesUtils.bookmarks.remove(bm);
    if (bookmark.keyword) {
      await PlacesUtils.keywords.remove(bookmark.keyword);
    }
  });
}

/**
 * Check that if the user hits enter and ctrl-t at the same time, we open the URL in the right tab.
 */
add_task(async function hitEnterLoadInRightTab() {
  info("Opening new tab");
  let oldTabCreatedPromise = BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "TabOpen");
  BrowserOpenTab();
  let oldTab = (await oldTabCreatedPromise).target;
  let oldTabLoadedPromise = BrowserTestUtils.browserLoaded(oldTab.linkedBrowser, false, TEST_URL);
  oldTabLoadedPromise.then(() => info("Old tab loaded"));
  let newTabCreatedPromise = BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "TabOpen");

  info("Creating bookmark and keyword");
  await addBookmark({title: "Test for keyword bookmark and URL", url: TEST_URL, keyword: "urlbarkeyword"});
  info("Filling URL bar, sending <return> and opening a tab");
  gURLBar.value = "urlbarkeyword";
  gURLBar.select();
  EventUtils.sendKey("return");
  BrowserOpenTab();
  info("Waiting for new tab");
  let newTab = (await newTabCreatedPromise).target;
  info("Created new tab; waiting for either tab to load");
  let newTabLoadedPromise = BrowserTestUtils.browserLoaded(newTab.linkedBrowser, false, TEST_URL);
  newTabLoadedPromise.then(() => info("New tab loaded"));
  await Promise.race([newTabLoadedPromise, oldTabLoadedPromise]);
  is(newTab.linkedBrowser.currentURI.spec, "about:newtab", "New tab still has about:newtab");
  is(oldTab.linkedBrowser.currentURI.spec, TEST_URL, "Old tab loaded URL");
  info("Closing new tab");
  BrowserTestUtils.removeTab(newTab);
  info("Closing old tab");
  BrowserTestUtils.removeTab(oldTab);
  info("Finished");
});
