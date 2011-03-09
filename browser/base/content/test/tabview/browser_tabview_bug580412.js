/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
  if (TabView.isVisible())
    onTabViewWindowLoaded();
  else
    TabView.show();
}

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);

  let contentWindow = document.getElementById("tab-view").contentWindow;
  let [originalTab] = gBrowser.visibleTabs;

  ok(TabView.isVisible(), "Tab View is visible");
  is(contentWindow.GroupItems.groupItems.length, 1, "There is only one group");
  let currentActiveGroup = contentWindow.GroupItems.getActiveGroupItem();

  // set double click interval to negative so quick drag and drop doesn't 
  // trigger the double click code.
  let origDBlClickInterval = contentWindow.UI.DBLCLICK_INTERVAL;
  contentWindow.UI.DBLCLICK_INTERVAL = -1;

  let endGame = function() {
    contentWindow.UI.reset();
    contentWindow.UI.DBLCLICK_INTERVAL = origDBlClickInterval;

    let onTabViewHidden = function() {
      window.removeEventListener("tabviewhidden", onTabViewHidden, false);
      ok(!TabView.isVisible(), "TabView is shown");
      finish();
    };
    window.addEventListener("tabviewhidden", onTabViewHidden, false);

    ok(TabView.isVisible(), "TabView is shown");

    gBrowser.selectedTab = originalTab;
    TabView.hide();
  }

  let part1 = function() {
    // move down 20 so we're far enough away from the top.
    checkSnap(currentActiveGroup, 0, 20, contentWindow, function(snapped){
      ok(!snapped,"Move away from the edge");

      // Just pick it up and drop it.
      checkSnap(currentActiveGroup, 0, 0, contentWindow, function(snapped){
        ok(!snapped,"Just pick it up and drop it");

        checkSnap(currentActiveGroup, 0, 1, contentWindow, function(snapped){
          ok(snapped,"Drag one pixel: should snap");

          checkSnap(currentActiveGroup, 0, 5, contentWindow, function(snapped){
            ok(!snapped,"Moving five pixels: shouldn't snap");
            endGame();
          });
        });
      });
    });
  }

  currentActiveGroup.setPosition(40, 40, true);
  currentActiveGroup.arrange({animate: false});
  part1();
}

function simulateDragDrop(tabItem, offsetX, offsetY, contentWindow) {
  // enter drag mode
  let dataTransfer;

  EventUtils.synthesizeMouse(
    tabItem.container, 1, 1, { type: "mousedown" }, contentWindow);
  let event = contentWindow.document.createEvent("DragEvents");
  event.initDragEvent(
    "dragenter", true, true, contentWindow, 0, 0, 0, 0, 0,
    false, false, false, false, 1, null, dataTransfer);
  tabItem.container.dispatchEvent(event);

  // drag over
  if (offsetX || offsetY) {
    let Ci = Components.interfaces;
    let utils = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor).
                              getInterface(Ci.nsIDOMWindowUtils);
    let rect = tabItem.getBounds();
    for (let i = 1; i <= 5; i++) {
      let left = rect.left + 1 + Math.round(i * offsetX / 5);
      let top = rect.top + 1 + Math.round(i * offsetY / 5);
      utils.sendMouseEvent("mousemove", left, top, 0, 1, 0);
    }
    event = contentWindow.document.createEvent("DragEvents");
    event.initDragEvent(
      "dragover", true, true, contentWindow, 0, 0, 0, 0, 0,
      false, false, false, false, 0, null, dataTransfer);
    tabItem.container.dispatchEvent(event);
  }

  // drop
  EventUtils.synthesizeMouse(
    tabItem.container, 0, 0, { type: "mouseup" }, contentWindow);
  event = contentWindow.document.createEvent("DragEvents");
  event.initDragEvent(
    "drop", true, true, contentWindow, 0, 0, 0, 0, 0,
    false, false, false, false, 0, null, dataTransfer);
  tabItem.container.dispatchEvent(event);
}

function checkSnap(item, offsetX, offsetY, contentWindow, callback) {
  let firstTop = item.getBounds().top;
  let firstLeft = item.getBounds().left;
  let onDrop = function() {
    let snapped = false;
    item.container.removeEventListener('drop', onDrop, false);
    if (item.getBounds().top != firstTop + offsetY)
      snapped = true;
    if (item.getBounds().left != firstLeft + offsetX)
      snapped = true;
    callback(snapped);
  };
  item.container.addEventListener('drop', onDrop, false);
  simulateDragDrop(item, offsetX, offsetY, contentWindow);
}
