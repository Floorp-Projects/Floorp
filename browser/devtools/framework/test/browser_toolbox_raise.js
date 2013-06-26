/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/Services.jsm");
let temp = {}
Cu.import("resource:///modules/devtools/gDevTools.jsm", temp);
let DevTools = temp.DevTools;

Cu.import("resource://gre/modules/devtools/Loader.jsm", temp);
let devtools = temp.devtools;

let Toolbox = devtools.Toolbox;

let toolbox, target, tab1, tab2;

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = tab1 = gBrowser.addTab();
  tab2 = gBrowser.addTab();
  target = TargetFactory.forTab(gBrowser.selectedTab);

  gBrowser.selectedBrowser.addEventListener("load", function onLoad(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, onLoad, true);
    gDevTools.showToolbox(target)
             .then(testBottomHost, console.error)
             .then(null, console.error);
  }, true);

  content.location = "data:text/html,test for opening toolbox in different hosts";
}

function testBottomHost(aToolbox) {
  toolbox = aToolbox;

  // switch to another tab and test toolbox.raise()
  gBrowser.selectedTab = tab2;
  executeSoon(function() {
    is(gBrowser.selectedTab, tab2, "Correct tab is selected before calling raise");
    toolbox.raise();
    executeSoon(function() {
      is(gBrowser.selectedTab, tab1, "Correct tab was selected after calling raise");

      toolbox.switchHost(Toolbox.HostType.WINDOW).then(testWindowHost).then(null, console.error);
    });
  });
}

function testWindowHost() {
  // Make sure toolbox is not focused.
  window.addEventListener("focus", onFocus, true);

  // Need to wait for focus  as otherwise window.focus() is overridden by
  // toolbox window getting focused first on Linux and Mac.
  let onToolboxFocus = () => {
    toolbox._host._window.removeEventListener("focus", onToolboxFocus, true);
    info("focusing main window.");
    window.focus()
  };
  // Need to wait for toolbox window to get focus.
  toolbox._host._window.addEventListener("focus", onToolboxFocus, true);
}

function onFocus() {
  info("Main window is focused before calling toolbox.raise()")
  window.removeEventListener("focus", onFocus, true);

  // Check if toolbox window got focus.
  toolbox._host._window.onfocus = () => {
    ok(true, "Toolbox window is the focused window after calling toolbox.raise()");
    cleanup();
  };
  // Now raise toolbox.
  toolbox.raise();
}

function cleanup() {
  Services.prefs.setCharPref("devtools.toolbox.host", Toolbox.HostType.BOTTOM);

  toolbox.destroy().then(function() {
    DevTools = Toolbox = toolbox = target = null;
    gBrowser.removeCurrentTab();
    gBrowser.removeCurrentTab();
    finish();
  });
}
