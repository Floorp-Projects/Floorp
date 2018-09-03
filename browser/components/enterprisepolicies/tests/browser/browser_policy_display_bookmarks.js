/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function setup() {
  await setupPolicyEngineWithJson({
    "policies": {
      "DisplayBookmarksToolbar": true,
    },
  });
});

add_task(async function test_menu_shown() {
  // Since testing will apply the policy after the browser has already started,
  // we will need to open a new window to actually see the toolbar
  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let menuBar = newWin.document.getElementById("PersonalToolbar");
  is(menuBar.getAttribute("collapsed"), "false",
     "The bookmarks toolbar should not be hidden");

  await BrowserTestUtils.closeWindow(newWin);
});
