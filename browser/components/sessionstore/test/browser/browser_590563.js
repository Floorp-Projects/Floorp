/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/Services.jsm");
let ss = Cc["@mozilla.org/browser/sessionstore;1"].
            getService(Ci.nsISessionStore);

let stateBackup = ss.getBrowserState();

function test() {
  waitForExplicitFinish();

  let oldState = {
    windows: [{
      tabs: [
        { entries: [{ url: "about:robots" }], hidden: true },
        { entries: [{ url: "about:blank" }], hidden: false }
      ]
    }]
  };
  let pageData = {
    url: "about:sessionrestore",
    formdata: { "#sessionData": "(" + JSON.stringify(oldState) + ")" }
  };
  let state = { windows: [{ tabs: [{ entries: [pageData] }] }] };

  // The form data will be restored before SSTabRestored, so we want to listen
  // for that on the currently selected tab (it will be reused)
  gBrowser.selectedTab.addEventListener("SSTabRestored", onSSTabRestored, true);

  ss.setBrowserState(JSON.stringify(state));
}

function onSSTabRestored(aEvent) {
  gBrowser.selectedTab.removeEventListener("SSTabRestored", onSSTabRestored, true);

  is(gBrowser.tabs.length, 1, "The total number of tabs should be 1");
  is(gBrowser.visibleTabs.length, 1, "The total number of visible tabs should be 1");

  executeSoon(middleClickTest);
}

function middleClickTest() {
  let tree = gBrowser.selectedBrowser.contentDocument.getElementById("tabList");
  is(tree.view.rowCount, 3, "There should be three items");

  let x = {}, y = {}, width = {}, height = {};

  // click on the first tab item
  tree.treeBoxObject.getCoordsForCellItem(1, tree.columns[1], "text", x, y, width, height);
  EventUtils.synthesizeMouse(tree.body, x.value, y.value, { button: 1 },
                             gBrowser.selectedBrowser.contentWindow);
  // click on the second tab item
  tree.treeBoxObject.getCoordsForCellItem(2, tree.columns[1], "text", x, y, width, height);
  EventUtils.synthesizeMouse(tree.body, x.value, y.value, { button: 1 },
                             gBrowser.selectedBrowser.contentWindow);

  is(gBrowser.tabs.length, 3,
     "The total number of tabs should be 3 after restoring 2 tabs by middle click.");
  is(gBrowser.visibleTabs.length, 3,
     "The total number of visible tabs should be 3 after restoring 2 tabs by middle click");

  cleanup();
}

function cleanup() {
   ss.setBrowserState(stateBackup);
   executeSoon(finish);
}
