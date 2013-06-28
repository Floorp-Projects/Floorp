/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let state = {
  windows: [{
    tabs: [{
      entries: [{ url: "about:mozilla" }],
      hidden: true,
      extData: {"tabview-tab": '{"url":"about:mozilla","groupID":1,"bounds":{"left":20,"top":20,"width":20,"height":20}}'}
    },{
      entries: [{ url: "about:mozilla" }],
      hidden: false,
      extData: {"tabview-tab": '{"url":"about:mozilla","groupID":2,"bounds":{"left":20,"top":20,"width":20,"height":20}}'},
    }],
    selected: 2,
    extData: {
      "tabview-groups": '{"nextID":3,"activeGroupId":2}',
      "tabview-group":
        '{"1":{"bounds":{"left":15,"top":5,"width":280,"height":232},"id":1},' +
        '"2":{"bounds":{"left":309,"top":5,"width":267,"height":226},"id":2}}'
    }
  }]
};

function test() {
  waitForExplicitFinish();

  newWindowWithState(state, function (win) {
    registerCleanupFunction(function () win.close());

    is(win.gBrowser.tabs.length, 2, "two tabs");

    let opts = {animate: true, byMouse: true};
    win.gBrowser.removeTab(win.gBrowser.visibleTabs[0], opts);

    let checkTabCount = function () {
      if (win.gBrowser.tabs.length > 1) {
        executeSoon(checkTabCount);
        return;
      }

      is(win.gBrowser.tabs.length, 1, "one tab");

      showTabView(function () {
        let cw = win.TabView.getContentWindow();
        is(cw.TabItems.items.length, 1, "one tabItem");

        waitForFocus(finish);
      }, win);
    };

    checkTabCount();
  });
}
