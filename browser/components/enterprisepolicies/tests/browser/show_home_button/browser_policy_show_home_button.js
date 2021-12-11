/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../head.js */

"use strict";

add_task(async function test_home_button_shown_boolean() {
  await setupPolicyEngineWithJson({
    policies: {
      ShowHomeButton: true,
    },
  });

  // Since testing will apply the policy after the browser has already started,
  // we will need to open a new window to actually see the menu bar
  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let homeButton = newWin.document.getElementById("home-button");
  isnot(homeButton, null, "The home button should be visible");

  await BrowserTestUtils.closeWindow(newWin);
});

add_task(async function test_home_button_hidden_boolean() {
  await setupPolicyEngineWithJson({
    policies: {
      ShowHomeButton: false,
    },
  });

  // Since testing will apply the policy after the browser has already started,
  // we will need to open a new window to actually see the menu bar
  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let homeButton = newWin.document.getElementById("home-button");
  is(homeButton, null, "The home button should be gone");

  await BrowserTestUtils.closeWindow(newWin);
});
