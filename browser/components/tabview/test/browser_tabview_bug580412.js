/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();
  showTabView(onTabViewShown);
}

function onTabViewShown() {
  let contentWindow = TabView.getContentWindow();
  let [originalTab] = gBrowser.visibleTabs;

  ok(TabView.isVisible(), "Tab View is visible");
  is(contentWindow.GroupItems.groupItems.length, 1, "There is only one group");
  let currentActiveGroup = contentWindow.GroupItems.getActiveGroupItem();

  let endGame = function() {
    ok(TabView.isVisible(), "TabView is shown");
    gBrowser.selectedTab = originalTab;

    hideTabView(function () {
      ok(!TabView.isVisible(), "TabView is hidden");
      finish();
    });
  }

  // we need to stop the setBounds() css animation or else the test will
  // fail in single-mode because the group is newly created "ontabshown".
  let $container = contentWindow.iQ(currentActiveGroup.container);
  $container.css("transition-property", "none");

  currentActiveGroup.setPosition(40, 40, true);
  currentActiveGroup.arrange({animate: false});

  // move down 20 so we're far enough away from the top.
  checkSnap(currentActiveGroup, 0, 20, contentWindow, function(snapped){
    is(currentActiveGroup.getBounds().top, 60, "group.top is 60px");
    ok(!snapped,"Move away from the edge");

    // Just pick it up and drop it.
    checkSnap(currentActiveGroup, 0, 0, contentWindow, function(snapped){
      is(currentActiveGroup.getBounds().top, 60, "group.top is 60px");
      ok(!snapped,"Just pick it up and drop it");

      checkSnap(currentActiveGroup, 0, 1, contentWindow, function(snapped){
        is(currentActiveGroup.getBounds().top, 60, "group.top is 60px");
        ok(snapped,"Drag one pixel: should snap");

        checkSnap(currentActiveGroup, 0, 5, contentWindow, function(snapped){
          is(currentActiveGroup.getBounds().top, 65, "group.top is 65px");
          ok(!snapped,"Moving five pixels: shouldn't snap");
          endGame();
        });
      });
    });
  });
}

function simulateDragDrop(item, offsetX, offsetY, contentWindow) {
  let target = item.container;

  EventUtils.synthesizeMouse(target, 1, 1, {type: "mousedown"}, contentWindow);
  EventUtils.synthesizeMouse(target, 1 + offsetX, 1 + offsetY, {type: "mousemove"}, contentWindow);
  EventUtils.synthesizeMouse(target, 1, 1, {type: "mouseup"}, contentWindow);
}

function checkSnap(item, offsetX, offsetY, contentWindow, callback) {
  let firstTop = item.getBounds().top;
  let firstLeft = item.getBounds().left;

  simulateDragDrop(item, offsetX, offsetY, contentWindow);

  let snapped = false;
  if (item.getBounds().top != firstTop + offsetY)
    snapped = true;
  if (item.getBounds().left != firstLeft + offsetX)
    snapped = true;

  callback(snapped);
}
