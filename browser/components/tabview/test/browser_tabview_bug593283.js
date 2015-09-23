/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const DUMMY_PAGE_URL = "http://example.com/";

var state = {
  windows: [{
    tabs: [{
      entries: [{ url: DUMMY_PAGE_URL }],
      hidden: false, 
      attributes: {},
      extData: {
        "tabview-tab": '{"url":"' + DUMMY_PAGE_URL + '","groupID":1,"title":null,"active":true}'
      }
    },{
      entries: [{ url: DUMMY_PAGE_URL }],
      hidden: false,
      attributes: {},
      extData: {
        "tabview-tab": '{"url":"' + DUMMY_PAGE_URL + '","groupID":1,"title":null}'
      }
    },{
      entries: [{ url: DUMMY_PAGE_URL }],
      hidden: true,
      attributes: {},
      extData: {
        "tabview-tab": '{"url":"' + DUMMY_PAGE_URL + '","groupID":2,"title":null}'
      },
    },{
      entries: [{ url: DUMMY_PAGE_URL }],
      hidden: true,
      attributes: {},
      extData: {
        "tabview-tab": '{"url":"' + DUMMY_PAGE_URL + '","groupID":2,"title":null,"active":true}'
      },
    }],
    selected:1,
    _closedTabs: [],
    extData: {
      "tabview-groups": '{"nextID":3,"activeGroupId":2,"totalNumber":2}',
      "tabview-group": 
        '{"1":{"bounds":{"left":15,"top":28,"width":546,"height":218},' + 
        '"userSize":{"x":546,"y":218},"title":"","id":1},' +
        '"2":{"bounds":{"left":15,"top":261,"width":546,"height":199},' + 
        '"userSize":{"x":546,"y":199},"title":"","id":2}}',
      "tabview-ui": '{"pageBounds":{"left":0,"top":0,"width":976,"height":663}}'
    }, sizemode:"normal"
  }]
};

function test() {
  waitForExplicitFinish();

  newWindowWithState(state, function (win) {
    registerCleanupFunction(() => win.close());

    showTabView(function() {
      let cw = win.TabView.getContentWindow();
      let groupItems = cw.GroupItems.groupItems;
      let groupOne = groupItems[0];
      let groupTwo = groupItems[1];

      // check the active tab of each group
      is(groupOne.getActiveTab(), groupOne.getChild(0), "The active tab item of group one is the first one");
      is(groupTwo.getActiveTab(), groupTwo.getChild(1), "The active tab item of group two is the second one");

      is(cw.UI.getActiveTab(), groupOne.getChild(0), "The hightlighted tab item is the first one in group one");
      // select a group and the second tab should be hightlighted
      cw.UI.setActive(groupTwo);
      is(cw.UI.getActiveTab(), groupTwo.getChild(1), "The hightlighted tab item is the second one in group two");

      finish();
    }, win);
  });
}
