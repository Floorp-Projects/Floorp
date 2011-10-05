/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  newWindowWithTabView(function(win) {
    let cw = win.TabView.getContentWindow();
    let groupItem = cw.GroupItems.groupItems[0];

    // set the group to 180 x 100 so it shows two tab items without going into
    // the stacked mode
    groupItem.setBounds(new cw.Rect(100, 0, 180, 100));

    hideTabView(function() {
      groupItem.addSubscriber("childAdded", function onChildAdded(data) {
        groupItem.removeSubscriber("childAdded", onChildAdded);

        is(groupItem.getChildren().length, 3, "The number of children in group is 3");
        ok(groupItem.isStacked(), "The group item is stacked after adding a new tab");

        let tabItem = groupItem.getChild(2);
        groupItem.addSubscriber("childRemoved", function onChildRemoved() {
          tabItem.removeSubscriber("childRemoved", onChildRemoved);

          is(groupItem.getChildren().length, 2, "The number of children in group is 2");
          // give a delay for the active item to be set
          executeSoon(function() {
            showTabView(function() {
              ok(!groupItem.isStacked(), "The group item is not stacked after removing a tab");
              finish();
            }, win);
          });
        });
        win.BrowserCloseTabOrWindow();
      });
      win.gBrowser.loadOneTab("", {inBackground: true});
    }, win);
  }, function(win) {
    registerCleanupFunction(function () {
      win.close();
    });

    win.gBrowser.addTab();
  });
}
