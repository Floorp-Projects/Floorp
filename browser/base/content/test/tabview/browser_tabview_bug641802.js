/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let openMoveToGroupPopup = function () {
    let tab = gBrowser.selectedTab;
    document.popupNode = tab;
    contextMenu.openPopup(tab, "end_after", 0, 0, true, false);
    tvMenuPopup.openPopup(tvMenu, "end_after", 0, 0, true, false);
  }

  let hideMoveToGroupPopup = function () {
    tvMenuPopup.hidePopup();
    contextMenu.hidePopup();
  }

  let assertValidPrerequisites = function (visible) {
    let cw = TabView.getContentWindow();
    is(cw.GroupItems.groupItems.length, 1, "there is one groupItem");
    is(gBrowser.tabs.length, 1, "there is one tab");
    is(TabView.isVisible(), visible, "tabview is visible");
  }

  let tvMenu = document.getElementById("context_tabViewMenu");
  let contextMenu = document.getElementById("tabContextMenu");
  let tvMenuPopup = document.getElementById("context_tabViewMenuPopup");

  waitForExplicitFinish();

  registerCleanupFunction(function () {
    hideMoveToGroupPopup();
    hideTabView();

    let groupItems = TabView.getContentWindow().GroupItems.groupItems;
    if (groupItems.length > 1)
      closeGroupItem(groupItems[0]);
  });

  showTabView(function () {
    assertValidPrerequisites(true);

    hideTabView(function () {
      let groupItem = createGroupItemWithBlankTabs(window, 200, 200, 10, 1);
      groupItem.setTitle("group2");

      gBrowser.selectedTab = gBrowser.tabs[0];

      executeSoon(function () {
        openMoveToGroupPopup();
        is(tvMenuPopup.firstChild.getAttribute("label"), "group2", "menuItem is present");
        hideMoveToGroupPopup();

        closeGroupItem(groupItem, function () {
          openMoveToGroupPopup();
          is(tvMenuPopup.firstChild.tagName, "menuseparator", "menuItem is not present");
          hideMoveToGroupPopup();

          assertValidPrerequisites(false);
          finish();
        });
      });
    });
  });
}
