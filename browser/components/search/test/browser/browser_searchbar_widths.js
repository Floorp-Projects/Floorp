/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that when the searchbar has a specific width, opening a new window
// honours that specific width.
add_task(async function test_searchbar_width_persistence() {
  await gCUITestUtils.addSearchBar();
  registerCleanupFunction(async function () {
    gCUITestUtils.removeSearchBar();
  });

  // Really, we should use the splitter, but drag/drop is hard and fragile in
  // tests, so let's just fake it real quick:
  let container = BrowserSearch.searchBar.parentNode;
  // There's no width attribute set initially, just grab the info from layout:
  let oldWidth = container.getBoundingClientRect().width;
  let newWidth = "" + Math.round(oldWidth * 2);
  container.setAttribute("width", newWidth);

  let win = await BrowserTestUtils.openNewBrowserWindow();
  let otherBar = win.BrowserSearch.searchBar;
  ok(otherBar, "Should have a search bar in the other window");
  if (otherBar) {
    is(
      otherBar.parentNode.getAttribute("width"),
      newWidth,
      "Should have matching width"
    );
  }
  await BrowserTestUtils.closeWindow(win);
});
