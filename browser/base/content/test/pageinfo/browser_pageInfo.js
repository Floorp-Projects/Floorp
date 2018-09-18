const URI = "https://example.com/browser/browser/base/content/test/pageinfo/feed_tab.html";

function test() {
  waitForExplicitFinish();

  var pageInfo;

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false,
                                 URI).then(() => {
    Services.obs.addObserver(observer, "page-info-dialog-loaded");
    pageInfo = BrowserPageInfo();
  });
  gBrowser.selectedBrowser.loadURI(URI);

  function observer(win, topic, data) {
    Services.obs.removeObserver(observer, "page-info-dialog-loaded");
    pageInfo.onFinished.push(handlePageInfo);
  }

  function handlePageInfo() {
    ok(pageInfo.document.getElementById("feedTab"), "Feed tab");
    let feedListbox = pageInfo.document.getElementById("feedListbox");
    ok(feedListbox, "Feed list should exist.");

    var feedRowsNum = feedListbox.getRowCount();
    is(feedRowsNum, 3, "Number of feeds listed should be correct.");

    for (var i = 0; i < feedRowsNum; i++) {
      let feedItem = feedListbox.getItemAtIndex(i);
      let feedTitle = feedItem.querySelector(".feedTitle");
      is(feedTitle.textContent, i + 1, "Feed name should be correct.");
    }

    pageInfo.close();
    gBrowser.removeCurrentTab();
    finish();
  }
}
