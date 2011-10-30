/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let state = {
  windows: [{
    tabs: [{
      entries: [{ url: "about:blank" }],
      hidden: true,
      extData: {"tabview-tab": '{"url":"about:blank","groupID":1,"bounds":{"left":120,"top":20,"width":20,"height":20}}'}
    },{
      entries: [{ url: "about:blank" }],
      hidden: false,
      extData: {"tabview-tab": '{"url":"about:blank","groupID":2,"bounds":{"left":20,"top":20,"width":20,"height":20}}'},
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

function test() {
  waitForExplicitFinish();

  newWindowWithState(state, function(win) {
    registerCleanupFunction(function() win.close());

    win.gBrowser.addTab();

    ok(win.gBrowser.tabs[0].hidden, "The first tab is hidden");
    win.gBrowser.selectedTab = win.gBrowser.tabs[0];

    function onTabViewFrameInitialized() {
      win.removeEventListener(
        "tabviewframeinitialized", onTabViewFrameInitialized, false);

      let cw = win.TabView.getContentWindow();
      is(cw.GroupItems.groupItem(1).getChild(0).tab, win.gBrowser.selectedTab, "The tab in group one matches the selected tab");
      is(cw.GroupItems.groupItem(1).getChildren().length, 1, "The group one has only one tab item");
      is(cw.GroupItems.groupItem(2).getChildren().length, 2, "The group one has only two tab item")

      finish();
    }
    win.addEventListener(
      "tabviewframeinitialized", onTabViewFrameInitialized, false);
  });
}

