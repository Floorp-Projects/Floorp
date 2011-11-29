/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  newWindowWithTabView(function(win) {
    let numTabsToUpdate = 2;

    showTabView(function() {
      let contentWindow = win.TabView.getContentWindow();
      let groupItem = contentWindow.GroupItems.groupItems[0];

      groupItem.getChildren().forEach(function(tabItem) {
        tabItem.addSubscriber("updated", function onUpdated() {
          tabItem.removeSubscriber("updated", onUpdated);

          if (--numTabsToUpdate == 0)
            finish();
        });
        contentWindow.TabItems.update(tabItem.tab);
      });
    }, win);
  }, function(win) {
    BrowserOffline.toggleOfflineStatus();
    ok(Services.io.offline, "It is now offline");

    let originalTab = win.gBrowser.tabs[0];
    originalTab.linkedBrowser.loadURI("http://www.example.com/foo");
    win.gBrowser.addTab("http://www.example.com/bar");

    registerCleanupFunction(function () {
      if (Services.io.offline)
        BrowserOffline.toggleOfflineStatus();
      win.close();
    });
  });
}

