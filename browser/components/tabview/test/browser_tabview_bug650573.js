/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);

let win;
let cw;
let stateBackup;

function test() {
  waitForExplicitFinish();

  newWindowWithTabView(
    function(newWin) {
      cw = win.TabView.getContentWindow();
      hideTabView(testRestoreNormal, win);
    },
    function(newWin) {
      win = newWin;

      stateBackup = ss.getWindowState(win);
      registerCleanupFunction(function () {
        win.close();
      });
    }
  );
}

function testRestoreNormal() {
  testRestore("normal", function () {
    waitForBrowserState(JSON.parse(stateBackup), testRestorePinned);
  });
}

function testRestorePinned() {
  win.gBrowser.loadOneTab("about:blank", {inBackground: true});
  win.gBrowser.pinTab(win.gBrowser.tabs[0]);

  testRestore("pinned", function () {
    waitForBrowserState(JSON.parse(stateBackup), testRestoreHidden);
  });
}

function testRestoreHidden() {
  showTabView(function() {
    let groupItem = createGroupItemWithBlankTabs(win, 200, 200, 20, 1);
    let tabItem = groupItem.getChild(0);

    hideGroupItem(groupItem, function () {
      testRestore("hidden", function () {
        isnot(tabItem.container.style.display, "none", "tabItem is visible");
        waitForFocus(finish);
      });
    });
  }, win);
}

function testRestore(prefix, callback) {
  waitForBrowserState(createBrowserState(), function () {
    is(win.gBrowser.tabs.length, 2, prefix + ": two tabs restored");

    is(cw.GroupItems.groupItems.length, 2, prefix + ": we have two groupItems");

    let [groupItem1, groupItem2] = cw.GroupItems.groupItems;
    is(groupItem1.id, "1st-group-id", prefix + ": groupItem1's ID is valid");
    is(groupItem1.getChildren().length, 1, prefix + ": groupItem1 has one child");

    is(groupItem2.id, "2nd-group-id", prefix + ": groupItem2's ID is valid");
    is(groupItem2.getChildren().length, 1, prefix + ": groupItem2 has one child");

    callback();
  });
}

function waitForBrowserState(state, callback) {
  whenWindowStateReady(win, function () {
    afterAllTabsLoaded(callback, win);
  });

  executeSoon(function() {
    ss.setWindowState(win, JSON.stringify(state), true);
  });
}

function createBrowserState() {
  let tabViewGroups = {nextID: 99, activeGroupId: 1};
  let tabViewGroup = {
    "1st-group-id": {bounds: {left: 20, top: 20, width: 200, height: 200}, title: "new group 1", id: "1st-group-id"},
    "2nd-group-id": {bounds: {left: 240, top: 20, width: 200, height: 200}, title: "new group 2", id: "2nd-group-id"}
  };

  let tab1Data = {bounds: {left: 240, top: 20, width: 20, height: 20}, url: "about:robots", groupID: "2nd-group-id"};
  let tab1 = {
    entries: [{url: "about:robots"}],
    extData: {"tabview-tab": JSON.stringify(tab1Data)}
  };

  let tab2Data = {bounds: {left: 20, top: 20, width: 20, height: 20}, url: "about:mozilla", groupID: "1st-group-id"};
  let tab2 = {
    entries: [{url: "about:mozilla"}],
    extData: {"tabview-tab": JSON.stringify(tab2Data)}
  };

  return {windows: [{
    tabs: [tab1, tab2],
    selected: 1,
    extData: {"tabview-groups": JSON.stringify(tabViewGroups),
    "tabview-group": JSON.stringify(tabViewGroup)}
  }]};
}
