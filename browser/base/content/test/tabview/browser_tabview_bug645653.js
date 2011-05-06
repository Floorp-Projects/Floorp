/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* Orphans a non-blank tab, duplicates it and checks whether a new group is created with two tabs.
 * The original one should be the first tab of the new group.
 *
 * This prevents overlaid tabs in Tab View (only one tab appears to be there).
 * In addition, as only one active orphaned tab is shown when Tab View is hidden
 * and there are two tabs shown after the duplication, it also prevents
 * the inactive tab to suddenly disappear when toggling Tab View twice.
 *
 * Covers:
 *   Bug 645653 - Middle-click on reload button to duplicate orphan tabs does not create a group
 *   Bug 643119 - Ctrl+Drag to duplicate does not work for orphaned tabs
 *   ... (and any other way of duplicating a non-blank orphaned tab).
 *
 * See tabitems.js::_reconnect() for the fix.
 *
 * Miguel Ojeda <miguel.ojeda.sandonis@gmail.com>
 */

function loadedAboutMozilla(tab) {
  return tab.linkedBrowser.contentDocument.getElementById('moztext');
}

function test() {
  waitForExplicitFinish();
  showTabView(function() {
    ok(TabView.isVisible(), "Tab View is visible");

    let contentWindow = TabView.getContentWindow();
    is(contentWindow.GroupItems.groupItems.length, 1, "There is one group item on startup.");

    let originalGroupItem = contentWindow.GroupItems.groupItems[0];
    is(originalGroupItem.getChildren().length, 1, "There is one tab item in that group.");

    let originalTabItem = originalGroupItem.getChild(0);
    ok(originalTabItem, "The tabitem has been found.");

    // close the group => orphan the tab
    originalGroupItem.close();
    contentWindow.UI.setActive(originalGroupItem);
    is(contentWindow.GroupItems.groupItems.length, 0, "There are not any groups now.");

    ok(TabView.isVisible(), "Tab View is still shown.");

    hideTabView(function() {
      ok(!TabView.isVisible(), "Tab View is not shown anymore.");

      // load a non-blank page
      loadURI('about:mozilla');

      afterAllTabsLoaded(function() {
        ok(loadedAboutMozilla(originalTabItem.tab), "The original tab loaded about:mozilla.");

        // duplicate it
        duplicateTabIn(originalTabItem.tab, "tabshift");

        afterAllTabsLoaded(function() {
          // check
          is(gBrowser.selectedTab, originalTabItem.tab, "The selected tab is the original one.");
          is(contentWindow.GroupItems.groupItems.length, 1, "There is one group item again.");
          let groupItem = contentWindow.GroupItems.groupItems[0];
          is(groupItem.getChildren().length, 2, "There are two tab items in that group.");
          is(originalTabItem, groupItem.getChild(0), "The first tab item in the group is the original one.");
          let otherTab = groupItem.getChild(1);
          ok(loadedAboutMozilla(otherTab.tab), "The other tab loaded about:mozilla.");

          // clean up
          gBrowser.removeTab(otherTab.tab);
          is(contentWindow.GroupItems.groupItems.length, 1, "There is one group item after closing the second tab.");
          is(groupItem.getChildren().length, 1, "There is only one tab item after closing the second tab.");
          is(originalTabItem, groupItem.getChild(0), "The first tab item in the group is still the original one.");
          loadURI("about:blank");
          afterAllTabsLoaded(function() {
            finish();
          });
        });
      });
    });
  });
}

