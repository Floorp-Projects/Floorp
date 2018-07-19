/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the "browser console" menu item opens or focuses (if already open)
// the console window instead of toggling it open/close.

"use strict";

const TEST_MESSAGE = "testmessage";
const { Tools } = require("devtools/client/definitions");

add_task(async function() {
  let currWindow, hud;

  const mainWindow = Services.wm.getMostRecentWindow(null);

  await HUDService.openBrowserConsoleOrFocus();

  hud = HUDService.getBrowserConsole();

  ok(hud.ui.document.hasFocus(), "Focus in the document");

  console.log(TEST_MESSAGE);

  await waitFor(() => findMessage(hud, TEST_MESSAGE));

  currWindow = Services.wm.getMostRecentWindow(null);
  is(currWindow.document.documentURI, Tools.webConsole.url,
     "The Browser Console is open and has focus");
  mainWindow.focus();
  await HUDService.openBrowserConsoleOrFocus();
  currWindow = Services.wm.getMostRecentWindow(null);
  is(currWindow.document.documentURI, Tools.webConsole.url,
     "The Browser Console is open and has focus");
  await HUDService.toggleBrowserConsole();
  hud = HUDService.getBrowserConsole();
  ok(!hud, "Browser Console has been closed");
});
