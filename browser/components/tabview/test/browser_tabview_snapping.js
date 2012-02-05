/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  newWindowWithTabView(onTabViewWindowLoaded, null, 1000, 800);
}

function onTabViewWindowLoaded(win) {
  let contentWindow = win.document.getElementById("tab-view").contentWindow;
  let [originalTab] = win.gBrowser.visibleTabs;

  ok(win.TabView.isVisible(), "Tab View is visible");
  is(contentWindow.GroupItems.groupItems.length, 1, "There is only one group");
  let currentActiveGroup = contentWindow.GroupItems.getActiveGroupItem();

  // Create a group
  // Note: 150 x 150 should be larger than the minimum size for a group item
  let firstBox = new contentWindow.Rect(80, 80, 160, 160);
  let firstGroup = new contentWindow.GroupItem([], { bounds: firstBox,
                                                     immediately: true });
  ok(firstGroup.getBounds().equals(firstBox), "This group got its bounds");
  
  // Create a second group
  let secondBox = new contentWindow.Rect(80, 280, 160, 160);
  let secondGroup = new contentWindow.GroupItem([], { bounds: secondBox,
                                                      immediately: true });
  ok(secondGroup.getBounds().equals(secondBox), "This second group got its bounds");
  
  // A third group is created later, but multiple functions need access to it.
  let thirdGroup = null;

  is(secondGroup.getBounds().top - firstGroup.getBounds().bottom, 40,
    "There's currently 40 px between the first group and second group");

  let endGame = function() {
    firstGroup.container.parentNode.removeChild(firstGroup.container);
    firstGroup.close();
    thirdGroup.container.parentNode.removeChild(thirdGroup.container);
    thirdGroup.close();

    win.close();
    ok(win.closed, "new window is closed");
    finish();
  }
  
  let continueWithPart2 = function() {
    
    ok(firstGroup.getBounds().equals(firstBox), "The first group should still have its bounds");
    
    // Create a third group
    let thirdBox = new contentWindow.Rect(80, 280, 200, 160);
    thirdGroup = new contentWindow.GroupItem([], { bounds: thirdBox,
                                                   immediately: true });
    ok(thirdGroup.getBounds().equals(thirdBox), "This third group got its bounds");
  
    is(thirdGroup.getBounds().top - firstGroup.getBounds().bottom, 40,
      "There's currently 40 px between the first group and third group");
  
    // Just move it to the left and drop it.
    checkSnap(thirdGroup, 0, 0, contentWindow, function(snapped){
      ok(!snapped,"Offset: Just move it to the left and drop it");
      
      // Move the second group up 10 px. It shouldn't snap yet.
      checkSnap(thirdGroup, 0, -10, contentWindow, function(snapped){
        ok(!snapped,"Offset: Moving up 10 should not snap");
  
        // Move the second group up 10 px. It now should snap.
        checkSnap(thirdGroup, 0, -10, contentWindow, function(snapped){
          ok(snapped,"Offset: Moving up 10 again should snap!");
          endGame();
        });
      });
    });
  };

  let part1 = function() {
    // Just pick it up and drop it.
    checkSnap(secondGroup, 0, 0, contentWindow, function(snapped){
      ok(!snapped,"Right under: Just pick it up and drop it");
      
      // Move the second group up 10 px. It shouldn't snap yet.
      checkSnap(secondGroup, 0, -10, contentWindow, function(snapped){
        ok(!snapped,"Right under: Moving up 10 should not snap");
  
        // Move the second group up 10 px. It now should snap.
        checkSnap(secondGroup, 0, -10, contentWindow, function(snapped){
          ok(snapped,"Right under: Moving up 10 again should snap!");
          // cheat by removing the second group, so that we disappear immediately
          secondGroup.container.parentNode.removeChild(secondGroup.container);
          secondGroup.close();
          continueWithPart2();
        });
      });
    });
  }
  
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

