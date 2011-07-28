/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  newWindowWithTabView(part1);
}

function part1(win) {
  registerCleanupFunction(function() win.close());

  let contentWindow = win.TabView.getContentWindow();
  is(contentWindow.GroupItems.groupItems.length, 1, "Has only one group");

  let originalTab = win.gBrowser.selectedTab;
  let originalGroup = contentWindow.GroupItems.groupItems[0];
  let newTab = win.gBrowser.loadOneTab("about:blank", {inBackground: true});
  
  is(originalGroup.getChildren().length, 2, "The original group now has two tabs");
  
  // create group two with the new tab
  let box = new contentWindow.Rect(300,300,150,150);
  let newGroup = new contentWindow.GroupItem([], {bounds: box, immediately: true});
  newGroup.add(newTab._tabViewTabItem, {immediately: true});

  // ensure active group item and tab
  contentWindow.UI.setActive(originalGroup);
  is(contentWindow.GroupItems.getActiveGroupItem(), originalGroup,
     "The original group is active");
  is(contentWindow.UI.getActiveTab(), originalTab._tabViewTabItem,
     "The original tab is active");
  
  function checkActive(callback, time) {
    is(contentWindow.GroupItems.getActiveGroupItem(), newGroup,
       "The new group is active");
    is(contentWindow.UI.getActiveTab(), newTab._tabViewTabItem,
       "The new tab is active");
    if (time)
      setTimeout(callback, time);
    else
      callback();
  }

  // click on the new tab, and check that the new tab and group are active
  // at two times: 10 ms after (still during the animation) and
  // 500 ms after (after the animation, hopefully). Either way, the new
  // tab and group should be active.
  EventUtils.sendMouseEvent({ type: "mousedown" },
                            newTab._tabViewTabItem.container, contentWindow);
  EventUtils.sendMouseEvent({ type: "mouseup" },
                            newTab._tabViewTabItem.container, contentWindow);
  setTimeout(function() {
    checkActive(function() {
      checkActive(function() {
        win.close();
        newWindowWithTabView(part2);
      });
    }, 490);
  }, 10)
}

function part2(win) {
  registerCleanupFunction(function() win.close());

  let newTab = win.gBrowser.loadOneTab("about:blank", {inBackground: true});
  hideTabView(function() {
    let selectedTab = win.gBrowser.selectedTab;
    isnot(selectedTab, newTab, "They are different tabs");

    // switch the selected tab to new tab
    win.gBrowser.selectedTab = newTab;

    showTabView(function () {
      hideTabView(function () {
        is(win.gBrowser.selectedTab, newTab,
           "The selected tab should be the same as before (new tab)");
        waitForFocus(finish);
      }, win);
    }, win);
  }, win);
}
