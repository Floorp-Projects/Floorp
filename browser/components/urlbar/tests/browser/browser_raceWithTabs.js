/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URL = `${TEST_BASE_URL}dummy_page.html`;

async function addBookmark(bookmark) {
  info("Creating bookmark and keyword");
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: bookmark.url,
    title: bookmark.title,
  });
  if (bookmark.keyword) {
    await PlacesUtils.keywords.insert({
      keyword: bookmark.keyword,
      url: bookmark.url,
    });
  }

  registerCleanupFunction(async function() {
    if (bookmark.keyword) {
      await PlacesUtils.keywords.remove(bookmark.keyword);
    }
    await PlacesUtils.bookmarks.remove(bm);
  });
}

/**
 * Check that if the user hits enter and ctrl-t at the same time, we open the
 * URL in the right tab.
 */
add_task(async function hitEnterLoadInRightTab() {
  await addBookmark({
    title: "Test for keyword bookmark and URL",
    url: TEST_URL,
    keyword: "urlbarkeyword",
  });

  info("Opening a tab");
  let oldTabOpenPromise = BrowserTestUtils.waitForEvent(
    gBrowser.tabContainer,
    "TabOpen"
  );
  BrowserOpenTab();
  let oldTab = (await oldTabOpenPromise).target;
  let oldTabLoadedPromise = BrowserTestUtils.browserLoaded(
    oldTab.linkedBrowser,
    false,
    TEST_URL
  ).then(() => info("Old tab loaded"));

  info("Filling URL bar, sending <return> and opening a tab");
  let tabOpenPromise = BrowserTestUtils.waitForEvent(
    gBrowser.tabContainer,
    "TabOpen"
  );
  gURLBar.value = "urlbarkeyword";
  gURLBar.focus();
  gURLBar.select();
  EventUtils.sendKey("return");

  info("Immediately open a second tab");
  BrowserOpenTab();
  let newTab = (await tabOpenPromise).target;

  info("Created new tab; waiting for tabs to load");
  let newTabLoadedPromise = BrowserTestUtils.browserLoaded(
    newTab.linkedBrowser,
    false,
    "about:newtab"
  ).then(() => info("New tab loaded"));
  // If one of the tabs loads the wrong page, this will timeout, and that
  // indicates we regressed this bug fix.
  await Promise.all([newTabLoadedPromise, oldTabLoadedPromise]);
  // These are not particularly useful, but the test must contain some checks.
  is(
    newTab.linkedBrowser.currentURI.spec,
    "about:newtab",
    "New tab loaded about:newtab"
  );
  is(oldTab.linkedBrowser.currentURI.spec, TEST_URL, "Old tab loaded URL");

  info("Closing tabs");
  BrowserTestUtils.removeTab(newTab);
  BrowserTestUtils.removeTab(oldTab);
});
