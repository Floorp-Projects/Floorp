/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */


function test() {
  waitForExplicitFinish();

  // create two groups and each group has one tab item
  let newState = {
    windows: [{
      tabs: [{
        entries: [{ url: "about:mozilla" }],
        hidden: true,
        attributes: {},
        extData: {
          "tabview-tab":
            '{"bounds":{"left":21,"top":29,"width":204,"height":153},' +
            '"userSize":null,"url":"about:mozilla","groupID":1,' +
            '"imageData":null,"title":null}'
        }
      },{
        entries: [{ url: "about:blank" }],
        hidden: false,
        attributes: {},
        extData: {
          "tabview-tab":
            '{"bounds":{"left":315,"top":29,"width":111,"height":84},' +
            '"userSize":null,"url":"about:blank","groupID":2,' +
            '"imageData":null,"title":null}'
        },
      }],
      selected:2,
      _closedTabs: [],
      extData: {
        "tabview-groups": '{"nextID":3,"activeGroupId":2}',
        "tabview-group":
          '{"1":{"bounds":{"left":15,"top":5,"width":280,"height":232},' +
          '"userSize":null,"title":"","id":1},' +
          '"2":{"bounds":{"left":309,"top":5,"width":267,"height":226},' +
          '"userSize":null,"title":"","id":2}}',
        "tabview-ui": '{"pageBounds":{"left":0,"top":0,"width":788,"height":548}}'
      }, sizemode:"normal"
    }]
  };

  newWindowWithState(newState, function(win) {
    registerCleanupFunction(() => win.close());

    let selectedTab = win.gBrowser.selectedTab;

    win.addEventListener("tabviewframeinitialized", function onInit() {
      win.removeEventListener("tabviewframeinitialized", onInit, false);
      // ensure the TabView.contentWindow is set.
      executeSoon(function() {
        let cw = win.TabView.getContentWindow();

        is(cw.GroupItems.groupItems.length, 2, "There are two groups");
        is(cw.GroupItems.getActiveGroupItem().id, 1, "The active group item is 1");
        isnot(selectedTab, win.gBrowser.selectedTab, "The selected tab is different");

        finish();
      });
    }, false);

    goToNextGroup(win);
  });
}
