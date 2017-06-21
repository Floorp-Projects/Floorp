/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URL = "data:text/html,test for opening toolbox in different hosts";

var {Toolbox} = require("devtools/client/framework/toolbox");

var toolbox, tab1, tab2;

function test() {
  addTab(TEST_URL).then(tab => {
    tab2 = BrowserTestUtils.addTab(gBrowser);
    let target = TargetFactory.forTab(tab);
    gDevTools.showToolbox(target)
             .then(testBottomHost, console.error)
             .catch(console.error);
  });
}

function testBottomHost(aToolbox) {
  toolbox = aToolbox;

  // switch to another tab and test toolbox.raise()
  gBrowser.selectedTab = tab2;
  executeSoon(function () {
    is(gBrowser.selectedTab, tab2, "Correct tab is selected before calling raise");
    toolbox.raise();
    executeSoon(function () {
      is(gBrowser.selectedTab, tab1, "Correct tab was selected after calling raise");

      toolbox.switchHost(Toolbox.HostType.WINDOW).then(testWindowHost).catch(console.error);
    });
  });
}

function testWindowHost() {
  // Make sure toolbox is not focused.
  window.addEventListener("focus", onFocus, true);

  // Need to wait for focus  as otherwise window.focus() is overridden by
  // toolbox window getting focused first on Linux and Mac.
  let onToolboxFocus = () => {
    toolbox.win.parent.removeEventListener("focus", onToolboxFocus, true);
    info("focusing main window.");
    window.focus();
  };
  // Need to wait for toolbox window to get focus.
  toolbox.win.parent.addEventListener("focus", onToolboxFocus, true);
}

function onFocus() {
  info("Main window is focused before calling toolbox.raise()");
  window.removeEventListener("focus", onFocus, true);

  // Check if toolbox window got focus.
  let onToolboxFocusAgain = () => {
    toolbox.win.parent.removeEventListener("focus", onToolboxFocusAgain);
    ok(true, "Toolbox window is the focused window after calling toolbox.raise()");
    cleanup();
  };
  toolbox.win.parent.addEventListener("focus", onToolboxFocusAgain);

  // Now raise toolbox.
  toolbox.raise();
}

function cleanup() {
  Services.prefs.setCharPref("devtools.toolbox.host", Toolbox.HostType.BOTTOM);

  toolbox.destroy().then(function () {
    toolbox = null;
    gBrowser.removeCurrentTab();
    gBrowser.removeCurrentTab();
    finish();
  });
}
