/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();
  newWindowWithTabView(onTabViewWindowLoaded);
}

function onTabViewWindowLoaded(win) {
  ok(win.TabView.isVisible(), "Tab View is visible");

  let contentWindow = win.document.getElementById("tab-view").contentWindow;
  let [originalTab] = win.gBrowser.visibleTabs;
  let originalGroup = contentWindow.GroupItems.getActiveGroupItem();

  // open a group with a tab, and close either the group or the tab.
  function openAndClose(groupOrTab, callback) {
    // let's create a group with a tab.
    let group = new contentWindow.GroupItem([], {
      immediately: true,
      bounds: {left: 20, top: 20, width: 400, height: 400}
    });
    contentWindow.UI.setActive(group);
    win.gBrowser.loadOneTab('about:blank', {inBackground: true});
  
    is(group.getChildren().length, 1, "The group has one child now.");
    let tab = group.getChild(0);
  
    function check() {
      if (groupOrTab == 'group') {
        group.removeSubscriber("groupHidden", check);
        group.closeHidden();
      } else
        tab.removeSubscriber("tabRemoved", check);
  
      is(contentWindow.GroupItems.getActiveGroupItem(), originalGroup,
        "The original group is active.");
      is(contentWindow.UI.getActiveTab(), originalTab._tabViewTabItem,
        "The original tab is active");
  
      callback();
    }
  
    if (groupOrTab == 'group') {
      group.addSubscriber("groupHidden", check);
      group.closeAll();
    } else {
      tab.addSubscriber("tabRemoved", check);
      tab.close();
    }
  }

  // PHASE 1: create a group with a tab and close the group.
  openAndClose("group", function postPhase1() {
    // PHASE 2: create a group with a tab and close the tab.
    openAndClose("tab", function postPhase2() {
      win.close();
      finish();
    });
  });
}
