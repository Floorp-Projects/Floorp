/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the Web Console doesn't leak when multiple tabs and windows are
// opened and then closed.

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for bug 595350";

var win1 = window, win2;
var openTabs = [];
var loadedTabCount = 0;

function test() {
  requestLongerTimeout(3);

  // Add two tabs in the main window.
  addTabs(win1);

  // Open a new window.
  win2 = OpenBrowserWindow();
  win2.addEventListener("load", onWindowLoad, true);
}

function onWindowLoad(aEvent) {
  win2.removeEventListener(aEvent.type, onWindowLoad, true);

  // Add two tabs in the new window.
  addTabs(win2);
}

function addTabs(aWindow) {
  for (let i = 0; i < 2; i++) {
    let tab = aWindow.gBrowser.addTab(TEST_URI);
    openTabs.push(tab);

    tab.linkedBrowser.addEventListener("load", function onLoad(aEvent) {
      tab.linkedBrowser.removeEventListener(aEvent.type, onLoad, true);

      loadedTabCount++;
      info("tabs loaded: " + loadedTabCount);
      if (loadedTabCount >= 4) {
        executeSoon(openConsoles);
      }
    }, true);
  }
}

function openConsoles() {
  function open(i) {
    let tab = openTabs[i];
    openConsole(tab).then(function (hud) {
      ok(hud, "HUD is open for tab " + i);
      let window = hud.target.tab.linkedBrowser.contentWindow;
      window.console.log("message for tab " + i);

      if (i >= openTabs.length - 1) {
        // Use executeSoon() to allow the promise to resolve.
        executeSoon(closeConsoles);
      }
      else {
        executeSoon(() => open(i + 1));
      }
    });
  }

  // open the Web Console for each of the four tabs and log a message.
  open(0);
}

function closeConsoles() {
  let consolesClosed = 0;

  function onWebConsoleClose(aSubject, aTopic) {
    if (aTopic == "web-console-destroyed") {
      consolesClosed++;
      info("consoles destroyed: " + consolesClosed);
      if (consolesClosed == 4) {
        // Use executeSoon() to allow all the observers to execute.
        executeSoon(finishTest);
      }
    }
  }

  Services.obs.addObserver(onWebConsoleClose, "web-console-destroyed");

  registerCleanupFunction(() => {
    Services.obs.removeObserver(onWebConsoleClose, "web-console-destroyed");
  });

  win2.close();

  win1.gBrowser.removeTab(openTabs[0]);
  win1.gBrowser.removeTab(openTabs[1]);

  openTabs = win1 = win2 = null;
}
