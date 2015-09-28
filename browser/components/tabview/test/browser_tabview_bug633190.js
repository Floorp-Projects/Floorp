/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var origTab = gBrowser.visibleTabs[0];
var contentWindow;

function test() {
  waitForExplicitFinish();

  test1();
}

// Open a new tab when the active tab item belongs to a group item.
function test1() {
  registerCleanupFunction(() => TabView.hide());

  showTabView(function() {
    ok(origTab._tabViewTabItem.parent, "The original tab belongs to a group");

    contentWindow = TabView.getContentWindow();
    contentWindow.UI.setActive(origTab._tabViewTabItem);

    testCreateTabAndThen(test2);
  });
}

// Open a new tab when the active tab item is nothing.
function test2() {
  showTabView(function() {
    contentWindow.UI.setActive(null, { onlyRemoveActiveTab: true });

    testCreateTabAndThen(test3);
  });
}

// Open a new tab when the active tab item is an orphan tab.
function test3() {
  showTabView(function() {
    let groupItem = origTab._tabViewTabItem.parent;
    let tabItems = groupItem.getChildren();
    is(tabItems.length, 3, "There are 3 tab items in the group");

    let lastTabItem = tabItems[tabItems.length - 1];
    groupItem.remove(lastTabItem);

    let orphanedTabs = contentWindow.GroupItems.getOrphanedTabs();
    is(orphanedTabs.length, 1, "There should be 1 orphan tab");
    is(orphanedTabs[0], lastTabItem, "The tab item is the same as the orphan tab");

    contentWindow.UI.setActive(lastTabItem);

    testCreateTabAndThen(function() {
      hideTabView(finish);
    });
  });
}

function testCreateTabAndThen(callback) {
  ok(TabView.isVisible(), "Tab View is visible");

  // detect tab open and zoomed in event.
  let onTabOpen = function(event) {
    gBrowser.tabContainer.removeEventListener("TabOpen", onTabOpen, false);

    // ensure that the default tabview listener is called before so the 
    // tab._tabViewTabItem exists
    executeSoon(function() {
      let tab = event.target;
      tabItem = tab._tabViewTabItem;
      ok(tabItem, "Tab item is available after tab open");

      registerCleanupFunction(() => gBrowser.removeTab(tab))

      tabItem.addSubscriber("zoomedIn", function onZoomedIn() {
        tabItem.removeSubscriber("zoomedIn", onZoomedIn);

        is(gBrowser.selectedTab, tab,
          "The selected tab is the same as the newly opened tab");
        executeSoon(callback);
      });
    });
  }
  gBrowser.tabContainer.addEventListener("TabOpen", onTabOpen, false);
  // use the menu item (the same as pressing cmd/ctrl + t)
  document.getElementById("menu_newNavigatorTab").doCommand();
}
