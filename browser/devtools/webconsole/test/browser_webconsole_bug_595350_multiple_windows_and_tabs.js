/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Patrick Walton <pcwalton@mozilla.com>
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

// Tests that the Web Console doesn't leak when multiple tabs and windows are
// opened and then closed.

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for bug 595350";

let win1 = window, win2;
let openTabs = [];
let loadedTabCount = 0;

function test() {
  requestLongerTimeout(2);

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
      if (loadedTabCount >= 4) {
        executeSoon(openConsoles);
      }
    }, true);
  }
}

function openConsoles() {
  // open the Web Console for each of the four tabs and log a message.
  let consolesOpen = 0;
  for (let i = 0; i < openTabs.length; i++) {
    let tab = openTabs[i];
    openConsole(tab, function(index, hud) {
      ok(hud, "HUD is open for tab " + index);
      let window = hud.target.tab.linkedBrowser.contentWindow;
      window.console.log("message for tab " + index);
      consolesOpen++;
    }.bind(null, i));
  }

  waitForSuccess({
    timeout: 15000,
    name: "4 web consoles opened",
    validatorFn: function()
    {
      return consolesOpen == 4;
    },
    successFn: closeConsoles,
    failureFn: closeConsoles,
  });
}

function closeConsoles() {
  let consolesClosed = 0;

  function onWebConsoleClose(aSubject, aTopic) {
    if (aTopic == "web-console-destroyed") {
      consolesClosed++;
    }
  }

  Services.obs.addObserver(onWebConsoleClose, "web-console-destroyed", false);

  win2.close();

  win1.gBrowser.removeTab(openTabs[0]);
  win1.gBrowser.removeTab(openTabs[1]);

  openTabs = win1 = win2 = null;

  function onTimeout() {
    Services.obs.removeObserver(onWebConsoleClose, "web-console-destroyed");
    executeSoon(finishTest);
  }

  waitForSuccess({
    timeout: 10000,
    name: "4 web consoles closed",
    validatorFn: function()
    {
      return consolesClosed == 4;
    },
    successFn: onTimeout,
    failureFn: onTimeout,
  });
}

