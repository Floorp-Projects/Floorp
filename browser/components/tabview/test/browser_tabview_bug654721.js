/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var state = {
  windows: [{
    tabs: [{
      entries: [{ url: "about:mozilla" }],
      hidden: true,
      extData: {"tabview-tab": '{"url":"about:mozilla","groupID":1,"bounds":{"left":20,"top":20,"width":20,"height":20}}'}
    },{
      entries: [{ url: "about:mozilla" }],
      hidden: false,
      // this is an existing orphan tab from a previous Fx version and we want
      // to make sure this gets transformed into a group
      extData: {"tabview-tab": '{"url":"about:mozilla","groupID":0,"bounds":{"left":300,"top":300,"width":200,"height":200}}'},
    }],
    selected: 2,
    extData: {
      "tabview-groups": '{"nextID":3,"activeGroupId":1}',
      "tabview-group":
        '{"1":{"bounds":{"left":20,"top":20,"width":200,"height":200},"id":1}}'
    }
  }]
};

function test() {
  waitForExplicitFinish();

  newWindowWithState(state, function (win) {
    registerCleanupFunction(() => win.close());

    showTabView(function () {
      let cw = win.TabView.getContentWindow();
      let groupItems = cw.GroupItems.groupItems;
      is(groupItems.length, 2, "two groupItems");

      let [group1, group2] = groupItems;

      let bounds1 = new cw.Rect(20, 20, 200, 200);
      ok(bounds1.equals(group1.getBounds()), "bounds for group1 are correct");

      let bounds2 = new cw.Rect(300, 300, 200, 200);
      ok(bounds2.equals(group2.getBounds()), "bounds for group2 are correct");

      cw.UI.setActive(group2);
      win.gBrowser.loadOneTab("about:blank", {inBackground: true});

      let tabItem = group2.getChild(0);
      let target = tabItem.container;

      EventUtils.synthesizeMouse(target, 10, 10, {type: 'mousedown'}, cw);
      EventUtils.synthesizeMouse(target, 20, -200, {type: 'mousemove'}, cw);
      EventUtils.synthesizeMouse(target, 10, 10, {type: 'mouseup'}, cw);

      is(groupItems.length, 3, "three groupItems");

      let latestGroup = groupItems[groupItems.length - 1];
      is(tabItem, latestGroup.getChild(0), "dragged tab has its own groupItem");

      finish();
    }, win);
  });
}
