const URL = "http://mochi.test:8888/browser/browser/base/content/test/general/feed_discovery.html";

/** Test for Bug 377611 **/

add_task(async function() {
  // Open a new tab.
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, URL);
  registerCleanupFunction(() => gBrowser.removeCurrentTab());

  let browser = gBrowser.selectedBrowser;
  await BrowserTestUtils.browserLoaded(browser);

  let discovered = browser.feeds;
  ok(discovered.length > 0, "some feeds should be discovered");

  let feeds = {};
  for (let aFeed of discovered) {
    feeds[aFeed.href] = true;
  }

  await ContentTask.spawn(browser, feeds, async function(contentFeeds) {
    for (let aLink of content.document.getElementsByTagName("link")) {
      // ignore real stylesheets, and anything without an href property
      if (aLink.type != "text/css" && aLink.href) {
        if (/bogus/i.test(aLink.title)) {
          ok(!contentFeeds[aLink.href], "don't discover " + aLink.href);
        } else {
          ok(contentFeeds[aLink.href], "should discover " + aLink.href);
        }
      }
    }
  });
});
