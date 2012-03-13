function test() {
  waitForExplicitFinish();

  var pageInfo;

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function loadListener() {
    gBrowser.selectedBrowser.removeEventListener("load", loadListener, true);

    Services.obs.addObserver(observer, "page-info-dialog-loaded", false);
    pageInfo = BrowserPageInfo();
  }, true);
  content.location =
    "https://example.com/browser/browser/base/content/test/feed_tab.html";

  function observer(win, topic, data) {
    Services.obs.removeObserver(observer, "page-info-dialog-loaded");
    handlePageInfo();
  }

  function handlePageInfo() {
    ok(pageInfo.document.getElementById("feedTab"), "Feed tab");
    let feedListbox = pageInfo.document.getElementById("feedListbox");
    ok(feedListbox, "Feed list");

    var feedRowsNum = feedListbox.getRowCount();
    is(feedRowsNum, 3, "Number of feeds listed");

    for (var i = 0; i < feedRowsNum; i++) {
      let feedItem = feedListbox.getItemAtIndex(i);
      is(feedItem.getAttribute("name"), i + 1, "Feed name");
    }

    pageInfo.close();
    gBrowser.removeCurrentTab();
    finish();
  }
}
