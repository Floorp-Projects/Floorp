/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  let tContextMenu = document.getElementById("tabContextMenu");
  let tvMenuPopup = document.getElementById("context_tabViewMenuPopup");
  let group1;
  let group2;

  function openMoveToGroupPopup() {
    let tab = gBrowser.selectedTab;
    let tvMenu = document.getElementById("context_tabViewMenu");
    let tvMenuPopup = document.getElementById("context_tabViewMenuPopup");
    let tvEvent = new Event("");

    tab.dispatchEvent(tvEvent);
    tContextMenu.openPopup(tab, "end_after", 0, 0, true, false, tvEvent);
    tvMenuPopup.openPopup(tvMenu, "end_after", 0, 0, true, false);
  }

  function hideMoveToGroupPopup() {
    tvMenuPopup.hidePopup();
    tContextMenu.hidePopup();
  }

  function createGroups() {
    let cw = TabView.getContentWindow();

    group1 = createGroupItemWithTabs(window, 200, 200, 20, ["about:blank"]);
    group1.setTitle("group with items and title");
    group2 = createEmptyGroupItem(cw, 200, 200, 20);
    cw.UI.setActive(cw.GroupItems.groupItems[0]);

    // Check the group count.
    is(cw.GroupItems.groupItems.length, 3, "Validate group count in tab view.");

    hideTabView(checkGroupMenuItems);
  }

  // The group count includes the separator and the 'new group' menu item.
  function checkGroupMenuItems() {
    // First test with an empty untitled group.
    openMoveToGroupPopup();
    is(tvMenuPopup.childNodes.length, 3, "Validate group item count in move to group popup.");
    hideMoveToGroupPopup();

    // Then test with an empty but titled group.
    group2.setTitle("empty group with title");
    openMoveToGroupPopup();
    is(tvMenuPopup.childNodes.length, 4, "Validate group item count in move to group popup.");
    hideMoveToGroupPopup();

    // Clean 
    closeGroupItem(group1, function() { closeGroupItem(group2, finish); });
  }

  showTabView(createGroups);
}

