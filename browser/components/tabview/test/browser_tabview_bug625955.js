/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();
  showTabView(onTabViewShown);
}

function onTabViewShown() {
  let contentWindow = TabView.getContentWindow();
  is(contentWindow.GroupItems.groupItems.length, 1, "Has one groupItem only");

  let groupItem = contentWindow.GroupItems.groupItems[0];
  let tabItems = groupItem.getChildren();
  is(tabItems.length, 1, "There is only one tabItems in the groupItem");

  let bounds = groupItem.bounds;
  gBrowser.addTab();
  gBrowser.removeTab(gBrowser.tabs[1]);
  ok(bounds.equals(groupItem.bounds), 'Group bounds recovered'); 

  is(tabItems.length, 1, "There is only one tabItem in the groupItem");

  hideTabView(finish);
}
