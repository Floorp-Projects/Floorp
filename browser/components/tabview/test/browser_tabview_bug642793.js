/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();
  newWindowWithTabView(testTopOfStack, loadTabs);
}

function loadTabs (win) {
  for (let i = 0; i < 4; i++)
    win.gBrowser.loadOneTab('about:blank', {inBackground: false});
  win.gBrowser.selectedTab = win.gBrowser.tabs[2];
}

function testTopOfStack(win) {
  registerCleanupFunction(function () { win.close(); });
  let cw = win.TabView.getContentWindow();
  let groupItem = cw.GroupItems.getActiveGroupItem();
  ok(!groupItem.isStacked(), 'groupItem is not stacked');
  groupItem.setSize(150, 150);
  groupItem.setUserSize();
  ok(groupItem.isStacked(), 'groupItem is now stacked');
  ok(groupItem.isTopOfStack(groupItem.getChild(2)),
    'the third tab is on top of stack');
  finish();
}

