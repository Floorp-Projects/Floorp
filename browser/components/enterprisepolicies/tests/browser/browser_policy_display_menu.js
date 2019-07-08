/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function setup() {
  await setupPolicyEngineWithJson({
    policies: {
      DisplayMenuBar: true,
    },
  });
});

add_task(async function test_menu_shown() {
  // Since testing will apply the policy after the browser has already started,
  // we will need to open a new window to actually see the menu bar
  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let menuBar = newWin.document.getElementById("toolbar-menubar");
  is(
    menuBar.getAttribute("autohide"),
    "false",
    "The menu bar should not be hidden"
  );

  await BrowserTestUtils.closeWindow(newWin);
});
