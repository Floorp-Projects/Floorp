/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

add_task(function* test() {
  let sidebarBox = document.getElementById("sidebar-box");
  is(sidebarBox.hidden, true, "The sidebar should be hidden");

  // Uncollapse the personal toolbar if needed.
  let toolbar = document.getElementById("PersonalToolbar");
  let wasCollapsed = toolbar.collapsed;
  if (wasCollapsed) {
    yield promiseSetToolbarVisibility(toolbar, true);
  }

  let sidebar = yield promiseLoadedSidebar("viewBookmarksSidebar");
  registerCleanupFunction(() => {
    SidebarUI.hide();
  });

  // Focus the tree and check if its controller is returned.
  let tree = sidebar.contentDocument.getElementById("bookmarks-view");
  tree.focus();

  let controller = doGetPlacesControllerForCommand("placesCmd_copy");
  let treeController = tree.controllers
                           .getControllerForCommand("placesCmd_copy");
  ok(controller == treeController, "tree controller was returned");

  // Open the context menu for a toolbar item, and check if the toolbar's
  // controller is returned.
  let toolbarItems = document.getElementById("PlacesToolbarItems");
  EventUtils.synthesizeMouse(toolbarItems.childNodes[0],
                             4, 4, { type: "contextmenu", button: 2 },
                             window);
  controller = doGetPlacesControllerForCommand("placesCmd_copy");
  let toolbarController = document.getElementById("PlacesToolbar")
                                  .controllers
                                  .getControllerForCommand("placesCmd_copy");
  ok(controller == toolbarController, "the toolbar controller was returned");

  document.getElementById("placesContext").hidePopup();

  // Now that the context menu is closed, try to get the tree controller again.
  tree.focus();
  controller = doGetPlacesControllerForCommand("placesCmd_copy");
  ok(controller == treeController, "tree controller was returned");

  if (wasCollapsed) {
    yield promiseSetToolbarVisibility(toolbar, false);
  }
});

function promiseLoadedSidebar(cmd) {
  return new Promise(resolve => {
    let sidebar = document.getElementById("sidebar");
    sidebar.addEventListener("load", function onLoad() {
      sidebar.removeEventListener("load", onLoad, true);
      resolve(sidebar);
    }, true);

    SidebarUI.show(cmd);
  });
}
