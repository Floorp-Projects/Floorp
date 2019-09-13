/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the "browser console" menu item opens or focuses (if already open)
// the console window instead of toggling it open/close.

"use strict";
requestLongerTimeout(2);

const TEST_MESSAGE = "testmessage";
const { Tools } = require("devtools/client/definitions");

add_task(async function() {
  info("Get main browser window");
  const mainWindow = Services.wm.getMostRecentWindow(null);

  info("Open the Browser Console");
  await BrowserConsoleManager.openBrowserConsoleOrFocus();

  let hud = BrowserConsoleManager.getBrowserConsole();
  await waitFor(() => hud.ui.document.hasFocus());
  ok(true, "Focus is in the Browser Console");

  info("Emit a log message to display it in the Browser Console");
  console.log(TEST_MESSAGE);
  await waitFor(() => findMessage(hud, TEST_MESSAGE));

  let currWindow = Services.wm.getMostRecentWindow(null);
  is(
    currWindow.document.documentURI,
    Tools.webConsole.url,
    "The Browser Console is open and has focus"
  );

  info("Focus the main browser window");
  mainWindow.focus();

  info("Focus the Browser Console window");
  await BrowserConsoleManager.openBrowserConsoleOrFocus();
  currWindow = Services.wm.getMostRecentWindow(null);
  is(
    currWindow.document.documentURI,
    Tools.webConsole.url,
    "The Browser Console is open and has focus"
  );

  info("Close the Browser Console");
  await BrowserConsoleManager.toggleBrowserConsole();
  hud = BrowserConsoleManager.getBrowserConsole();
  ok(!hud, "Browser Console has been closed");
});
