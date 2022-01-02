/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_menu_shown_boolean() {
  await setupPolicyEngineWithJson({
    policies: {
      DisplayMenuBar: true,
    },
  });

  // Since testing will apply the policy after the browser has already started,
  // we will need to open a new window to actually see the menu bar
  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let menubar = newWin.document.getElementById("toolbar-menubar");
  is(
    menubar.getAttribute("autohide"),
    "false",
    "The menu bar should not be hidden"
  );

  await BrowserTestUtils.closeWindow(newWin);
});

add_task(async function test_menu_shown_string() {
  await setupPolicyEngineWithJson({
    policies: {
      DisplayMenuBar: "default-on",
    },
  });

  // Since testing will apply the policy after the browser has already started,
  // we will need to open a new window to actually see the menu bar
  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let menubar = newWin.document.getElementById("toolbar-menubar");
  is(
    menubar.getAttribute("autohide"),
    "false",
    "The menu bar should not be hidden"
  );

  await BrowserTestUtils.closeWindow(newWin);
});

add_task(async function test_menubar_on() {
  await setupPolicyEngineWithJson({
    policies: {
      DisplayMenuBar: "always",
    },
  });

  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let menubar = newWin.document.getElementById("toolbar-menubar");
  is(
    menubar.hasAttribute("inactive"),
    false,
    "Menu bar should not have inactive"
  );
  is(
    menubar.hasAttribute("toolbarname"),
    false,
    "Menu bar should not have a toolbarname"
  );
  await BrowserTestUtils.closeWindow(newWin);
});

add_task(async function test_menubar_off() {
  await setupPolicyEngineWithJson({
    policies: {
      DisplayMenuBar: "never",
    },
  });

  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let menubar = newWin.document.getElementById("toolbar-menubar");
  is(menubar.hasAttribute("inactive"), true, "Menu bar should have inactive");
  is(
    menubar.hasAttribute("toolbarname"),
    false,
    "Menu bar should not have a toolbarname"
  );
  await BrowserTestUtils.closeWindow(newWin);
});
