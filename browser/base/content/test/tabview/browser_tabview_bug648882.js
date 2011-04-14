/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

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
    let interval = cw.UI._maxInteractiveWait - 10;

    let simulateDragDrop = function (target) {
      EventUtils.synthesizeMouse(target, 5, 5, {type: "mousedown"}, cw);
      EventUtils.synthesizeMouse(target, 40, 20, {type: "mousemove"}, cw);
      EventUtils.synthesizeMouse(target, 20, 20, {type: "mouseup"}, cw);
    }

    let moveGroup = function () {
      simulateDragDrop(container);

      if (!--numLoops) {
        numLoops = 10;
        win.clearInterval(intervalID);
        intervalID = win.setInterval(resizeGroup, interval);
      }
    };

    let resizeGroup = function () {
      simulateDragDrop(resizer);

      if (!--numLoops) {
        isIdle = true;
        win.clearInterval(intervalID);
      }
    };

    SimpleTest.waitForFocus(function () {
      cw.TabItems.pausePainting();
      cw.TabItems.update(tab);

      tabItem.addSubscriber(tabItem, "updated", function () {
        tabItem.removeSubscriber(tabItem, "updated");
        ok(isIdle, "tabItem is updated only when UI is idle");
        finish();
      });

      intervalID = win.setInterval(moveGroup, interval);
      registerCleanupFunction(function () win.clearInterval(intervalID));

      moveGroup();
      cw.TabItems.resumePainting();
    }, cw);
  });
}
