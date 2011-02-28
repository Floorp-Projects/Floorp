/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  newWindowWithTabView(onTabViewWindowLoaded);
}

function onTabViewWindowLoaded(win) {
  win.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);

  ok(win.TabView.isVisible(), "Tab View is visible");

  let contentWindow = win.document.getElementById("tab-view").contentWindow;
  let [originalTab] = win.gBrowser.visibleTabs;

  let currentGroup = contentWindow.GroupItems.getActiveGroupItem();

  currentGroup.setSize(600, 600, true);
  currentGroup.setUserSize();

  let down1 = function down1(resized) {
    checkResized(currentGroup, 50, 50, false, "A little bigger", up1, contentWindow, win);
  };
  
  let up1 = function up1(resized) {
    checkResized(currentGroup, -400, -400, true, "Much smaller", down2, contentWindow, win);    
  }

  let down2 = function down2(resized) {
    checkResized(currentGroup, 400, 400, undefined,
      "Bigger after much smaller: TODO (bug 625668): the group should be resized!",
      up2, contentWindow, win);
  };
  
  let up2 = function up2(resized) {
    checkResized(currentGroup, -400, -400, undefined,
      "Much smaller: TODO (bug 625668): the group should be resized!",
      down3, contentWindow, win);    
  }

  let down3 = function down3(resized) {
    // reset the usersize of the group, so this should clear the "cramped" feeling.
    currentGroup.setSize(100,100,true);
    currentGroup.setUserSize();
    checkResized(currentGroup, 400, 400, false,
      "After clearing the cramp",
      up3, contentWindow, win);
  };
  
  let up3 = function up3(resized) {
    win.close();
    finish();
  }

  // start by making it a little smaller.
  checkResized(currentGroup, -50, -50, false, "A little smaller", down1, contentWindow, win);
}

function simulateResizeBy(xDiff, yDiff, win) {
  win = win || window;

  win.resizeBy(xDiff, yDiff);
}

function checkResized(item, xDiff, yDiff, expectResized, note, callback, contentWindow, win) {
  let originalBounds = new contentWindow.Rect(item.getBounds());
  simulateResizeBy(xDiff, yDiff, win);

  let newBounds = item.getBounds();
  let resized = !newBounds.equals(originalBounds);
  if (expectResized !== undefined)
    is(resized, expectResized, note + ": The group should " + 
      (expectResized ? "" : "not ") + "be resized");
  callback(resized);
}
