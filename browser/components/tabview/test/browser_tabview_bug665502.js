/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  let onLoad = function (win) {
    for (let i = 0; i < 2; i++)
      win.gBrowser.addTab();
  };

  let onShow = function (win) {
    registerCleanupFunction(() => win.close());

    let cw = win.TabView.getContentWindow();
    let groupItem = cw.GroupItems.groupItems[0];

    groupItem.setSize(400, 200, true);

    let tabItem = groupItem.getChild(0);
    let bounds = tabItem.getBounds();

    is(groupItem.getActiveTab(), tabItem, "the first tab is active");
    EventUtils.synthesizeMouseAtCenter(tabItem.container, {button: 1}, cw);

    is(groupItem.getChildren().indexOf(tabItem), -1, "tabItem got removed");
    ok(bounds.equals(groupItem.getChild(0).getBounds()), "tabItem bounds didn't change");

    finish();
  };

  newWindowWithTabView(onShow, onLoad);
}
