/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

let tab1, tab2, win1, win2;
let noErrors = true;

function tab1Loaded(aEvent) {
  browser.removeEventListener(aEvent.type, tab1Loaded, true);

  win2 = OpenBrowserWindow();
  win2.addEventListener("load", win2Loaded, true);
}

function win2Loaded(aEvent) {
  win2.removeEventListener(aEvent.type, win2Loaded, true);

  tab2 = win2.gBrowser.addTab(TEST_URI);
  win2.gBrowser.selectedTab = tab2;
  tab2.linkedBrowser.addEventListener("load", tab2Loaded, true);
}

function tab2Loaded(aEvent) {
  tab2.linkedBrowser.removeEventListener(aEvent.type, tab2Loaded, true);

  let consolesOpened = 0;
  function onWebConsoleOpen() {
    consolesOpened++;
    if (consolesOpened == 2) {
      Services.obs.removeObserver(onWebConsoleOpen, "web-console-created");
      executeSoon(closeConsoles);
    }
  }

  Services.obs.addObserver(onWebConsoleOpen, "web-console-created", false);

  function openConsoles() {
    try {
      let target1 = TargetFactory.forTab(tab1);
      gDevTools.showToolbox(target1, "webconsole");
    }
    catch (ex) {
      ok(false, "gDevTools.showToolbox(target1) exception: " + ex);
      noErrors = false;
    }

    try {
      let target2 = TargetFactory.forTab(tab2);
      gDevTools.showToolbox(target2, "webconsole");
    }
    catch (ex) {
      ok(false, "gDevTools.showToolbox(target2) exception: " + ex);
      noErrors = false;
    }
  }

  let consolesClosed = 0;
  function onWebConsoleClose()
  {
    consolesClosed++;
    if (consolesClosed == 2) {
      Services.obs.removeObserver(onWebConsoleClose, "web-console-destroyed");
      executeSoon(testEnd);
    }
  }

  function closeConsoles() {
    Services.obs.addObserver(onWebConsoleClose, "web-console-destroyed", false);

    try {
      let target1 = TargetFactory.forTab(tab1);
      gDevTools.closeToolbox(target1);
    }
    catch (ex) {
      ok(false, "gDevTools.closeToolbox(target1) exception: " + ex);
      noErrors = false;
    }

    try {
      let target2 = TargetFactory.forTab(tab2);
      gDevTools.closeToolbox(target2);
    }
    catch (ex) {
      ok(false, "gDevTools.closeToolbox(target2) exception: " + ex);
      noErrors = false;
    }
  }

  function testEnd() {
    ok(noErrors, "there were no errors");

    Array.forEach(win1.gBrowser.tabs, function(aTab) {
      win1.gBrowser.removeTab(aTab);
    });
    Array.forEach(win2.gBrowser.tabs, function(aTab) {
      win2.gBrowser.removeTab(aTab);
    });

    executeSoon(function() {
      win2.close();
      tab1 = tab2 = win1 = win2 = null;
      finishTest();
    });
  }

  waitForFocus(openConsoles, tab2.linkedBrowser.contentWindow);
}

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", tab1Loaded, true);
  tab1 = gBrowser.selectedTab;
  win1 = window;
}

