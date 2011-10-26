/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  showTabView(function () {
    let [tab] = gBrowser.tabs;
    let groupId = tab._tabViewTabItem.parent.id;

    TabView.moveTabTo(tab, groupId);
    is(tab._tabViewTabItem.parent.id, groupId, 'tab did not change its group');

    hideTabView(finish);
  });
}
