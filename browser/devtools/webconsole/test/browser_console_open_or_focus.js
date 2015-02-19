/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test that the "browser console" menu item opens or focuses (if already open)
// the console window instead of toggling it open/close.


"use strict";

let test = asyncTest(function* () {
  let wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                   .getService(Components.interfaces.nsIWindowMediator);
  let currWindow, hud, mainWindow;

  mainWindow = Services.wm.getMostRecentWindow(null);

  yield HUDService.openBrowserConsoleOrFocus();

  hud = HUDService.getBrowserConsole();

  console.log("testmessage");
  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "testmessage"
    }],
  });

  currWindow = Services.wm.getMostRecentWindow(null);
  is(currWindow.document.documentURI, devtools.Tools.webConsole.url,
     "The Browser Console is open and has focus");

  mainWindow.focus();

  yield HUDService.openBrowserConsoleOrFocus();

  currWindow = Services.wm.getMostRecentWindow(null);
  is(currWindow.document.documentURI, devtools.Tools.webConsole.url,
     "The Browser Console is open and has focus");

  yield HUDService.toggleBrowserConsole();

  hud = HUDService.getBrowserConsole();
  ok(!hud, "Browser Console has been closed");
});
