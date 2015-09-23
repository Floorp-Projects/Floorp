/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  const DUMMY_PAGE_URL = "about:blank";
  const DUMMY_PAGE_URL_2 = "http://mochi.test:8888/";

  // create two groups and each group has one tab item
  let newState = {
    windows: [{
      tabs: [{
        entries: [{ url: DUMMY_PAGE_URL }],
        hidden: true,
        attributes: {},
        extData: {
          "tabview-tab":
            '{"bounds":{"left":21,"top":29,"width":204,"height":153},' +
            '"userSize":null,"url":"' + DUMMY_PAGE_URL + '","groupID":1,' +
            '"imageData":null,"title":null}'
        }
      },{
        entries: [{ url: DUMMY_PAGE_URL_2 }],
        hidden: false,
        attributes: {},
        extData: {
          "tabview-tab":
            '{"bounds":{"left":315,"top":29,"width":111,"height":84},' +
            '"userSize":null,"url":"' + DUMMY_PAGE_URL_2 + '","groupID":2,' +
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

    whenTabViewIsShown(function() {
      let contentWindow = win.TabView.getContentWindow();

      is(contentWindow.GroupItems.groupItems.length, 2, "There are still two groups");
      is(win.gBrowser.tabs.length, 1, "There is only one tab");

      finish();
    }, win);
    win.gBrowser.removeTab(win.gBrowser.selectedTab);
  });
}

