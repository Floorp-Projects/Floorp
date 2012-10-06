/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  function moveTabOutOfGroup(aTab, aGroup, aCW, aCallback) {
    let tabPos = aTab.getBounds().center();
    let groupPos = aGroup.getBounds();
    let groupPosX = (groupPos.left + groupPos.right) / 2;
    let groupPosY = groupPos.bottom + 200;
    let offsetX = Math.round(groupPosX - tabPos.x);
    let offsetY = Math.round(groupPosY - tabPos.y);

    simulateDragDrop(aTab, offsetX, offsetY, aCW);
  }

  function moveTabInGroup(aTab, aIndexTo, aGroup, aCW) {
    let tabTo = aGroup.getChild(aIndexTo);
    let tabPos = aTab.getBounds().center();
    let tabToPos = tabTo.getBounds().center();
    let offsetX = Math.round(tabToPos.x - tabPos.x);
    let offsetY = Math.round(tabToPos.y - tabPos.y);

    simulateDragDrop(aTab, offsetX, offsetY, aCW);
  }

  function getTabNumbers(aGroup) {
    return aGroup.getChildren().map(function (child) {
      let url = child.tab.linkedBrowser.currentURI.spec;
      return url.replace("about:blank#", "");
    }).join(",");
  }

  function moveTabs(aGroup, aCW) {
    // Test 1: Move the last tab to the third position.
    let tab = aGroup.getChild(6);
    moveTabInGroup(tab, 2, aGroup, aCW);
    is(getTabNumbers(aGroup), "0,1,6,2,3,4,5", "Validate tab positions in test 1.");

    // Test 2: Move the second tab to the end of the list.
    tab = aGroup.getChild(1);
    moveTabInGroup(tab, 6, aGroup, aCW);
    is(getTabNumbers(aGroup), "0,6,2,3,4,5,1", "Validate tab positions in test 2.");

    // Test 3: Move the fifth tab outside the group.
    tab = aGroup.getChild(4);
    moveTabOutOfGroup(tab, aGroup, aCW);
    is(getTabNumbers(aGroup), "0,6,2,3,5,1", "Validate tab positions in test 3.");
    is(aCW.GroupItems.groupItems.length, 3, "Validate group count in test 3.");

    // This test is disabled because it is fragile -- see bug 797975
    if (false) {
      // Test 4: Move the fifth tab back into the group, on the second row.
      waitForTransition(tab, function() {
	moveTabInGroup(tab, 4, aGroup, aCW);
	is(getTabNumbers(aGroup), "0,6,2,3,4,5,1", "Validate tab positions in test 4.");
	is(aCW.GroupItems.groupItems.length, 2, "Validate group count in test 4.");
	closeGroupItem(aGroup, function() { hideTabView(finish) });
      });
    } else {
      closeGroupItem(aGroup, function() { hideTabView(finish) });
    }
  }

  function createGroup(win) {
    registerCleanupFunction(function() win.close());
    let cw = win.TabView.getContentWindow();
    let group = createGroupItemWithTabs(win, 400, 430, 100, [
      "about:blank#0", "about:blank#1", "about:blank#2", "about:blank#3",
      "about:blank#4", "about:blank#5", "about:blank#6"
    ]);
    let groupSize = group.getChildren().length;

    is(cw.GroupItems.groupItems.length, 2, "Validate group count in tab view.");
    ok(!group.shouldStack(groupSize), "Check that group should not stack.");
    is(group._columns, 3, "Check the there should be three columns.");

    moveTabs(group, cw);
  }

  newWindowWithTabView(createGroup);
}

function simulateDragDrop(aTab, aOffsetX, aOffsetY, aCW) {
  let target = aTab.container;
  let rect = target.getBoundingClientRect();
  let startX = (rect.right - rect.left) / 2;
  let startY = (rect.bottom - rect.top) / 2;
  let steps = 2;
  let incrementX = aOffsetX / steps;
  let incrementY = aOffsetY / steps;

  EventUtils.synthesizeMouse(
    target, startX, startY, { type: "mousedown" }, aCW);
  for (let i = 1; i <= steps; i++) {
    EventUtils.synthesizeMouse(
      target, incrementX + startX, incrementY + startY,
      { type: "mousemove" }, aCW);
  };
  EventUtils.synthesizeMouseAtCenter(target, { type: "mouseup" }, aCW);
}

function waitForTransition(aTab, aCallback) {
  let groupContainer = aTab.parent.container;
  groupContainer.addEventListener("transitionend", function onTransitionEnd(aEvent) {
    if (aEvent.target == groupContainer) {
      groupContainer.removeEventListener("transitionend", onTransitionEnd);
      executeSoon(aCallback);
    }
  });
}
