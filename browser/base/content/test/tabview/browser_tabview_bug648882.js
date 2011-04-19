/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  testHeartbeatAfterBusyUI(function () {
    testTabUpdatesWithBusyUI(finish);
  });
}

function testHeartbeatAfterBusyUI(callback) {
  newWindowWithTabView(function (win) {
    registerCleanupFunction(function () win.close());

    let cw = win.TabView.getContentWindow();
    let tab = win.gBrowser.tabs[0];
    let tabItem = tab._tabViewTabItem;

    cw.TabItems.pausePainting();

    tabItem.addSubscriber(tabItem, "updated", function () {
      tabItem.removeSubscriber(tabItem, "updated");
      callback();
    });

    cw.TabItems.update(tab);

    cw.UI.isIdle = function () {
      cw.UI.isIdle = function () true;
      return false;
    }

    cw.TabItems.resumePainting();
  });
}

function testTabUpdatesWithBusyUI(callback) {
  newWindowWithTabView(function (win) {
    registerCleanupFunction(function () win.close());

    let cw = win.TabView.getContentWindow();
    let tab = win.gBrowser.tabs[0];
    let tabItem = tab._tabViewTabItem;
    let groupItem = cw.GroupItems.groupItems[0];
    let container = groupItem.container;
    let resizer = groupItem.$resizer[0];

    let intervalID;
    let isIdle = false;
    let numLoops = 10;
    let interval = cw.UI._maxInteractiveWait / 2;

    let simulateBusyUI = function () {
      let target = 5 < numLoops-- ? container : resizer;

      EventUtils.synthesizeMouse(target, 5, 5, {type: "mousedown"}, cw);
      EventUtils.synthesizeMouse(target, 40, 20, {type: "mousemove"}, cw);
      EventUtils.synthesizeMouse(target, 20, 20, {type: "mouseup"}, cw);

      win.setTimeout(function () {
        if (numLoops)
          simulateBusyUI();
        else
          isIdle = true;
      }, interval);
    };

    SimpleTest.waitForFocus(function () {
      cw.TabItems.pausePainting();
      cw.TabItems.update(tab);

      tabItem.addSubscriber(tabItem, "updated", function () {
        tabItem.removeSubscriber(tabItem, "updated");
        ok(isIdle, "tabItem is updated only when UI is idle");
        callback();
      });

      simulateBusyUI();
      win.setTimeout(simulateBusyUI, interval);
      cw.TabItems.resumePainting();
    }, cw);
  });
}
