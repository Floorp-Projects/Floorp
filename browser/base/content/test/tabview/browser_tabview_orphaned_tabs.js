/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let tabOne;
let newWin;

function test() {
  waitForExplicitFinish();

  newWindowWithTabView(onTabViewWindowLoaded, function(win) {
    newWin = win;
    tabOne = newWin.gBrowser.addTab();
  });
}

function onTabViewWindowLoaded() {
  newWin.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);

  ok(newWin.TabView.isVisible(), "Tab View is visible");

  let contentWindow = newWin.document.getElementById("tab-view").contentWindow;

  // 1) the tab should belong to a group, and no orphan tabs
  ok(tabOne._tabViewTabItem.parent, "Tab one belongs to a group");
  is(contentWindow.GroupItems.getOrphanedTabs().length, 0, "No orphaned tabs");

  // 2) create a group, add a blank tab 
  let groupItem = createEmptyGroupItem(contentWindow, 300, 300, 200);

  let onTabViewHidden = function() {
    newWin.removeEventListener("tabviewhidden", onTabViewHidden, false);

    // 3) the new group item should have one child and no orphan tab
    is(groupItem.getChildren().length, 1, "The group item has an item");
    is(contentWindow.GroupItems.getOrphanedTabs().length, 0, "No orphaned tabs");
    
    let checkAndFinish = function() {
      // 4) check existence of stored group data for tab before finishing
      let tabData = contentWindow.Storage.getTabData(tabItem.tab, function () {});
      ok(tabData && contentWindow.TabItems.storageSanity(tabData) && tabData.groupID, 
         "Tab two has stored group data");

      // clean up and finish the test
      newWin.gBrowser.removeTab(tabOne);
      newWin.gBrowser.removeTab(tabItem.tab);
      whenWindowObservesOnce(newWin, "domwindowclosed", function() {
        finish();
      });
      newWin.close();
    };
    let tabItem = groupItem.getChild(0);
    // the item may not be connected so subscriber would be used in that case.
    if (tabItem._reconnected) {
      checkAndFinish();
    } else {
      tabItem.addSubscriber(tabItem, "reconnected", function() {
        tabItem.removeSubscriber(tabItem, "reconnected");
        checkAndFinish();
      });
    }
  };
  newWin.addEventListener("tabviewhidden", onTabViewHidden, false);

  // click on the + button
  let newTabButton = groupItem.container.getElementsByClassName("newTabButton");
  ok(newTabButton[0], "New tab button exists");

  EventUtils.sendMouseEvent({ type: "click" }, newTabButton[0], contentWindow);
}

function whenWindowObservesOnce(win, topic, callback) {
  let windowWatcher = 
    Cc["@mozilla.org/embedcomp/window-watcher;1"].getService(Ci.nsIWindowWatcher);
  function windowObserver(subject, topicName, aData) {
    if (win == subject.QueryInterface(Ci.nsIDOMWindow) && topic == topicName) {
      windowWatcher.unregisterNotification(windowObserver);
      callback();
    }
  }
  windowWatcher.registerNotification(windowObserver);
}
