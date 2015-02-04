/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function test() {
  waitForExplicitFinish();

  let sidebarBox = document.getElementById("sidebar-box");
  is(sidebarBox.hidden, true, "The sidebar should be hidden");

  // Uncollapse the personal toolbar if needed.
  let toolbar = document.getElementById("PersonalToolbar");
  let wasCollapsed = toolbar.collapsed;
  if (wasCollapsed)
    setToolbarVisibility(toolbar, true);

  let sidebar = document.getElementById("sidebar");
  sidebar.addEventListener("load", function() {
    sidebar.removeEventListener("load", arguments.callee, true);
    let tree = sidebar.contentDocument.getElementById("bookmarks-view");

    // The Places view is build on load, so we enqueue our test.
    executeSoon(function() {
      // Focus the tree and check if its controller is returned.
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

      toggleSidebar();
      if (wasCollapsed)
        setToolbarVisibility(toolbar, false);

      finish();
    });
  }, true);
  toggleSidebar("viewBookmarksSidebar", true);
}
