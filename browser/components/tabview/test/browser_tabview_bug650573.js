/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);
let stateBackup = ss.getBrowserState();

function test() {
  waitForExplicitFinish();

  registerCleanupFunction(function () {
    ss.setBrowserState(stateBackup);
  });

  TabView._initFrame(function() {
    executeSoon(testRestoreNormal);
  });
}

function testRestoreNormal() {
  testRestore("normal", function () {
    waitForBrowserState(JSON.parse(stateBackup), testRestorePinned);
  });
}

function testRestorePinned() {
  gBrowser.loadOneTab("about:blank", {inBackground: true});
  gBrowser.pinTab(gBrowser.tabs[0]);

  testRestore("pinned", function () {
    waitForBrowserState(JSON.parse(stateBackup), testRestoreHidden);
  });
}

function testRestoreHidden() {
  let groupItem = createGroupItemWithBlankTabs(window, 20, 20, 20, 1);
  let tabItem = groupItem.getChild(0);

  hideGroupItem(groupItem, function () {
    testRestore("hidden", function () {
      isnot(tabItem.container.style.display, "none", "tabItem is visible");
      waitForFocus(finish);
    });
  });
}

function testRestore(prefix, callback) {
  waitForBrowserState(createBrowserState(), function () {
    is(gBrowser.tabs.length, 2, prefix + ": two tabs restored");

    let cw = TabView.getContentWindow();
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
  window.addEventListener("SSWindowStateReady", function onReady() {
    window.removeEventListener("SSWindowStateReady", onReady, false);
    executeSoon(callback);
  }, false);

  ss.setBrowserState(JSON.stringify(state));
}

function createBrowserState() {
  let bounds = {left: 20, top: 20, width: 20, height: 20};

  let tabViewGroups = {nextID: 99, activeGroupId: 1};
  let tabViewGroup = {
    "1st-group-id": {bounds: bounds, title: "new group 1", id: "1st-group-id"},
    "2nd-group-id": {bounds: bounds, title: "new group 2", id: "2nd-group-id"}
  };

  let tab1Data = {bounds: bounds, url: "about:rights", groupID: "2nd-group-id"};
  let tab1 = {
    entries: [{url: "about:rights"}],
    extData: {"tabview-tab": JSON.stringify(tab1Data)}
  };

  let tab2Data = {bounds: bounds, url: "about:mozilla", groupID: "1st-group-id"};
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
