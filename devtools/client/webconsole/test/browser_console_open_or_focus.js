/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that the "browser console" menu item opens or focuses (if already open)
// the console window instead of toggling it open/close.

"use strict";

var {Tools} = require("devtools/client/definitions");

add_task(function* () {
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
  is(currWindow.document.documentURI, Tools.webConsole.url,
     "The Browser Console is open and has focus");

  mainWindow.focus();

  yield HUDService.openBrowserConsoleOrFocus();

  currWindow = Services.wm.getMostRecentWindow(null);
  is(currWindow.document.documentURI, Tools.webConsole.url,
     "The Browser Console is open and has focus");

  yield HUDService.toggleBrowserConsole();

  hud = HUDService.getBrowserConsole();
  ok(!hud, "Browser Console has been closed");
});
