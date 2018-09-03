/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function setup() {
  await setupPolicyEngineWithJson({
    "policies": {
      "SearchBar": "separate",
    },
  });
});

add_task(async function test_menu_shown() {
  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let placement =  CustomizableUI.getPlacementOfWidget("search-container");
  isnot(placement, null, "Search bar has a placement");
  is(placement.area, CustomizableUI.AREA_NAVBAR, "Search bar is in the nav bar");
  await BrowserTestUtils.closeWindow(newWin);
});

add_task(async function setup() {
  await setupPolicyEngineWithJson({
    "policies": {
      "SearchBar": "unified",
    },
  });
});

add_task(async function test_menu_shown() {
  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let placement =  CustomizableUI.getPlacementOfWidget("search-container");
  is(placement, null, "Search bar has no placement");
  await BrowserTestUtils.closeWindow(newWin);
});
