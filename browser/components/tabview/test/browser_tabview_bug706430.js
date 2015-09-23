/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var state1 = {
  windows: [{
    tabs: [{
      entries: [{ url: "about:robots#1" }],
      hidden: true,
      extData: {"tabview-tab": '{"url":"about:robots#1","groupID":1,"bounds":{"left":120,"top":20,"width":20,"height":20}}'}
    },{
      entries: [{ url: "about:robots#2" }],
      hidden: false,
      extData: {"tabview-tab": '{"url":"about:robots#2","groupID":2,"bounds":{"left":20,"top":20,"width":20,"height":20}}'},
    }],
    selected: 2,
    extData: {
      "tabview-groups": '{"nextID":3,"activeGroupId":2, "totalNumber":2}',
      "tabview-group":
        '{"1":{"bounds":{"left":15,"top":5,"width":280,"height":232},"id":1},' +
        '"2":{"bounds":{"left":309,"top":5,"width":267,"height":226},"id":2}}'
    }
  }]
};

var state2 = {
  windows: [{
    tabs: [{entries: [{ url: "about:robots#1" }], hidden: true},
           {entries: [{ url: "about:robots#2" }], hidden: false}],
    selected: 2
  }]
};

var ss = Cc["@mozilla.org/browser/sessionstore;1"]
         .getService(Ci.nsISessionStore);

function test() {
  waitForExplicitFinish();

  newWindowWithState(state1, function (win) {
    registerCleanupFunction(() => win.close());

    showTabView(function () {
      let cw = win.TabView.getContentWindow();
      let [group1, group2] = cw.GroupItems.groupItems;
      let [tab1, tab2] = win.gBrowser.tabs;

      checkUrl(group1.getChild(0), "about:robots#1", "tab1 is in first group");
      checkUrl(group2.getChild(0), "about:robots#2", "tab2 is in second group");

      whenWindowStateReady(win, function () {
        let groups = cw.GroupItems.groupItems;
        is(groups.length, 1, "one groupItem");
        is(groups[0].getChildren().length, 2, "single groupItem has two children");

        finish();
      });

      ss.setWindowState(win, JSON.stringify(state2), true);
    }, win);
  });
}

function checkUrl(aTabItem, aUrl, aMsg) {
  is(aTabItem.tab.linkedBrowser.currentURI.spec, aUrl, aMsg);
}
