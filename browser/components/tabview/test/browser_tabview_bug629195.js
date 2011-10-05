/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let win;
let contentWindow;
let originalTab;

function test() {
  waitForExplicitFinish();

  newWindowWithTabView(
    function() {
      ok(win.TabView.isVisible(), "Tab View is visible");

      contentWindow = win.document.getElementById("tab-view").contentWindow;
      is(contentWindow.GroupItems.groupItems.length, 1, "There is one group");
      is(contentWindow.GroupItems.groupItems[0].getChildren().length, 1,
         "The group has only one tab item");

      // show the undo close group button
      let group = contentWindow.GroupItems.groupItems[0];
      hideGroupItem(group, function () restore(group.id));
    },
    function(newWin) {
      win = newWin;
      originalTab = win.gBrowser.visibleTabs[0];
      win.gBrowser.addTab();
      win.gBrowser.pinTab(originalTab);
    }
  );
}

function restore(groupId) {
  // window state ready
  let handleSSWindowStateReady = function() {
    win.removeEventListener("SSWindowStateReady", handleSSWindowStateReady, false);

    executeSoon(function() { 
      is(contentWindow.GroupItems.groupItems.length, 1, "There is one group");

      let group = contentWindow.GroupItems.groupItems[0];
      ok(!group.hidden, "The group is visible");
      is(group.getChildren().length, 2, "This group has two tab items");

      // check the position of the group item and the tab items.
      let tabItemOne = group.getChildren()[0];
      let tabItemTwo = group.getChildren()[1];

      let groupBounds = group.getBounds();
      let tabItemOneBounds = tabItemOne.getBounds();
      let tabItemTwoBounds = tabItemTwo.getBounds();

      ok(groupBounds.left < tabItemOneBounds.left && 
         (groupBounds.right) > (tabItemOneBounds.right) &&
         groupBounds.top < tabItemOneBounds.top && 
         (groupBounds.bottom) > (tabItemOneBounds.bottom), 
         "Tab item one is within the group");

      ok(groupBounds.left < tabItemOneBounds.left && 
         (groupBounds.right) > (tabItemTwoBounds.right) &&
         groupBounds.top < tabItemOneBounds.top && 
         (groupBounds.bottom) > (tabItemTwoBounds.bottom), 
         "Tab item two is within the group");

      win.close();
      finish();
    });
  }
  win.addEventListener("SSWindowStateReady", handleSSWindowStateReady, false);

  // simulate restoring previous session (one group and two tab items)
  const DUMMY_PAGE_URL = "http://example.com/";
  let newState = {
    windows: [{
      tabs: [{
        entries: [{ url: DUMMY_PAGE_URL }],
        index: 2,
        hidden: false,
        attributes: {},
        extData: {
          "tabview-tab":
            '{"bounds":{"left":208,"top":54,"width":205,"height":169},' +
            '"userSize":null,"url":"' + DUMMY_PAGE_URL + '","groupID":' + 
            groupId + ',"imageData":null,"title":null}'
        }}, {
        entries: [{ url: DUMMY_PAGE_URL }],
        index: 1,
        hidden: false,
        attributes: {},
        extData: {
          "tabview-tab":
            '{"bounds":{"left":429,"top":54,"width":205,"height":169},' +
            '"userSize":null,"url":"' + DUMMY_PAGE_URL + '","groupID":' + 
            groupId + ',"imageData":null,"title":null}'
        }
      }],
      extData: {
        "tabview-groups": '{"nextID":' + (groupId + 1) + ',"activeGroupId":' + groupId + '}',
        "tabview-group": 
          '{"' + groupId + '":{"bounds":{"left":202,"top":30,"width":455,"height":249},' + 
          '"userSize":null,"locked":{},"title":"","id":' + groupId +'}}',
        "tabview-ui": '{"pageBounds":{"left":0,"top":0,"width":788,"height":548}}'
      }, sizemode:"normal"
    }]
  };
  let ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);
  ss.setWindowState(win, JSON.stringify(newState), true);
}

