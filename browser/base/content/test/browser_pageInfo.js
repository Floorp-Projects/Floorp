function test() {
  waitForExplicitFinish();

  var pageInfo, obs, atTest = 0;
  var gTestPage = gBrowser.addTab();
  gBrowser.selectedTab = gTestPage;
  gTestPage.linkedBrowser.addEventListener("load", handleLoad, true);
  content.location =
    "https://example.com/browser/browser/base/content/test/feed_tab.html";
  gTestPage.focus();

  var observer = {
    observe: function(win, topic, data) {
      if (topic != "page-info-dialog-loaded")
        return;

      switch(atTest) {
        case 0:
          atTest++;
          handlePageInfo();
          break;
        case 1:
          atTest++;
          pageInfo = win;
          testLockClick();
          break;
        case 2:
          atTest++;
          obs.removeObserver(observer, "page-info-dialog-loaded");
          testLockDoubleClick();
          break;
      }
    }
  }

  function handleLoad() {
    this.removeEventListener("load", handleLoad, true);
    pageInfo = BrowserPageInfo();
    obs = Components.classes["@mozilla.org/observer-service;1"]
            .getService(Components.interfaces.nsIObserverService);
      obs.addObserver(observer, "page-info-dialog-loaded", false);
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

    pageInfo.addEventListener("unload", function() {
      pageInfo.removeEventListener("unload", arguments.callee, false);
      var lockIcon = document.getElementById("security-button");
      EventUtils.synthesizeMouse(lockIcon, 0, 0, {clickCount: 1});
    }, false);
    pageInfo.close();
  }

  function testLockClick() {
    var deck = $("mainDeck");
    is(deck.selectedPanel.id, "securityPanel", "The security tab should open when the lock icon is clicked");
    pageInfo.addEventListener("unload", function() {
      pageInfo.removeEventListener("unload", arguments.callee, false);
      var lockIcon = document.getElementById("security-button");
      EventUtils.synthesizeMouse(lockIcon, 0, 0, {clickCount: 1});
      EventUtils.synthesizeMouse(lockIcon, 0, 0, {clickCount: 2});
    }, false);
    pageInfo.close();
  }

  function testLockDoubleClick() {
    var pageInfoDialogs = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                                    .getService(Components.interfaces.nsIWindowMediator)
                                    .getEnumerator("Browser:page-info");
    var i = 0;
    while(pageInfoDialogs.hasMoreElements()) {
      i++;
      pageInfo = pageInfoDialogs.getNext();
      pageInfo.close();
    }
    is(i, 1, "When the lock is clicked twice there should be only one page info dialog");
    gTestPage.focus();
    gBrowser.removeCurrentTab();
    finish();
  }
}
