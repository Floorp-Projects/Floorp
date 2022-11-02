/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Since testing will apply the policy after the browser has already started,
// we will need to open a new window to actually see the toolbar

add_task(async function test_personaltoolbar_shown_old() {
  await setupPolicyEngineWithJson({
    policies: {
      DisplayBookmarksToolbar: true,
    },
  });
  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let menuBar = newWin.document.getElementById("PersonalToolbar");
  is(
    menuBar.getAttribute("collapsed"),
    "false",
    "The bookmarks toolbar should not be hidden"
  );

  await BrowserTestUtils.closeWindow(newWin);
});

add_task(async function test_personaltoolbar_shown() {
  await setupPolicyEngineWithJson({
    policies: {
      DisplayBookmarksToolbar: "always",
    },
  });

  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let menuBar = newWin.document.getElementById("PersonalToolbar");
  is(
    menuBar.getAttribute("collapsed"),
    "false",
    "The bookmarks toolbar should not be hidden"
  );

  await BrowserTestUtils.closeWindow(newWin);
});

add_task(async function test_personaltoolbar_hidden() {
  await setupPolicyEngineWithJson({
    policies: {
      DisplayBookmarksToolbar: "never",
    },
  });

  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let menuBar = newWin.document.getElementById("PersonalToolbar");
  is(
    menuBar.getAttribute("collapsed"),
    "true",
    "The bookmarks toolbar should be hidden"
  );

  await BrowserTestUtils.closeWindow(newWin);
});

add_task(async function test_personaltoolbar_newtabonly() {
  await setupPolicyEngineWithJson({
    policies: {
      DisplayBookmarksToolbar: "newtab",
    },
  });

  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let menuBar = newWin.document.getElementById("PersonalToolbar");
  is(
    menuBar.getAttribute("collapsed"),
    "true",
    "The bookmarks toolbar should be hidden"
  );

  await BrowserTestUtils.openNewForegroundTab(newWin.gBrowser, "about:newtab");
  menuBar = newWin.document.getElementById("PersonalToolbar");
  is(
    menuBar.getAttribute("collapsed"),
    "false",
    "The bookmarks toolbar should not be hidden"
  );

  await BrowserTestUtils.closeWindow(newWin);
});
