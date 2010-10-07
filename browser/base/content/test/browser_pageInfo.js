function test() {
  waitForExplicitFinish();

  var pageInfo;
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function () {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    pageInfo = BrowserPageInfo();
    Services.obs.addObserver(observer, "page-info-dialog-loaded", false);
  }, true);
  content.location =
    "https://example.com/browser/browser/base/content/test/feed_tab.html";

  function observer(win, topic, data) {
    if (topic != "page-info-dialog-loaded")
      return;

    handlePageInfo();
  }

  function $(aId) { return pageInfo.document.getElementById(aId) };

  function handlePageInfo() {
    var feedTab = $("feedTab");
    var feedListbox = $("feedListbox");

    ok(feedListbox, "Feed list is null (feeds tab is broken)");

    var feedRowsNum = feedListbox.getRowCount();

    ok(feedRowsNum == 3, "Number of feeds listed: " +
                         feedRowsNum + ", should be 3");


    for (var i = 0; i < feedRowsNum; i++) {
      let feedItem = feedListbox.getItemAtIndex(i);
      ok(feedItem.getAttribute("name") == (i+1), 
         "Name given: " + feedItem.getAttribute("name") + ", should be " + (i+1));
    }

    pageInfo.close();
    gBrowser.removeCurrentTab();
    finish();
  }
}
