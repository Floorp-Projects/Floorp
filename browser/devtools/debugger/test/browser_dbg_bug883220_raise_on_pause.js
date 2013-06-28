/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that debugger's tab is highlighted when it is paused and not the
// currently selected tool.

var gTab = null;
var gTab2 = null;
var gDebugger = null;
var gToolbox = null;
var gToolboxTab = null;
var gFocusedWindow = null;
Promise._reportErrors = true;

function test() {
  debug_tab_pane(STACK_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebugger = aPane.panelWin;
    gToolbox = aPane._toolbox;
    gToolboxTab = gToolbox.doc.getElementById("toolbox-tab-jsdebugger");
    gBrowser.selectedTab = gTab2 = gBrowser.addTab();
    executeSoon(function() {
      is(gBrowser.selectedTab, gTab2, "Debugger's tab is not the selected tab.");
      gFocusedWindow = window;
      testPause();
    });
  });
}

function focusMainWindow() {
  // Make sure toolbox is not focused.
  window.addEventListener("focus", onFocus, true);

  // execute soon to avoid any race conditions between toolbox and main window
  // getting focused.
  executeSoon(() => {
    window.focus();
  });
}

function onFocus() {
  window.removeEventListener("focus", onFocus, true);
  info("main window focused.")
  gFocusedWindow = window;
  testPause();
}

function testPause() {
  is(gDebugger.DebuggerController.activeThread.paused, false,
    "Should be running after debug_tab_pane.");

  is(gFocusedWindow, window, "Main window is the top level window before pause");

  if (gToolbox.hostType == devtools.Toolbox.HostType.WINDOW) {
    gToolbox._host._window.onfocus = () => {
      gFocusedWindow = gToolbox._host._window;
    };
  }

  gDebugger.DebuggerController.activeThread.addOneTimeListener("paused", function() {
    Services.tm.currentThread.dispatch({ run: function() {

      if (gToolbox.hostType == devtools.Toolbox.HostType.WINDOW) {
        is(gFocusedWindow, gToolbox._host._window,
           "Toolbox window is the top level window on pause.");
      }
      else {
        is(gBrowser.selectedTab, gTab, "Debugger's tab got selected.");
      }
      gToolbox.selectTool("webconsole").then(() => {
        ok(gToolboxTab.classList.contains("highlighted"),
           "The highlighted class is present");
        ok(!gToolboxTab.hasAttribute("selected") ||
           gToolboxTab.getAttribute("selected") != "true",
           "The tab is not selected");
      }).then(() => gToolbox.selectTool("jsdebugger")).then(() => {
        ok(gToolboxTab.classList.contains("highlighted"),
           "The highlighted class is present");
        ok(gToolboxTab.hasAttribute("selected") &&
           gToolboxTab.getAttribute("selected") == "true",
           "and the tab is selected, so the orange glow will not be present.");
      }).then(testResume);
    }}, 0);
  });

  EventUtils.sendMouseEvent({ type: "mousedown" },
    gDebugger.document.getElementById("resume"),
    gDebugger);
}

function testResume() {
  gDebugger.DebuggerController.activeThread.addOneTimeListener("resumed", function() {
    Services.tm.currentThread.dispatch({ run: function() {

      gToolbox.selectTool("webconsole").then(() => {
        ok(!gToolboxTab.classList.contains("highlighted"),
           "The highlighted class is not present now after the resume");
        ok(!gToolboxTab.hasAttribute("selected") ||
           gToolboxTab.getAttribute("selected") != "true",
           "The tab is not selected");
      }).then(maybeEndTest);
    }}, 0);
  });

  EventUtils.sendMouseEvent({ type: "mousedown" },
    gDebugger.document.getElementById("resume"),
    gDebugger);
}

function maybeEndTest() {
  if (gToolbox.hostType == devtools.Toolbox.HostType.WINDOW) {
    gToolbox.switchHost(devtools.Toolbox.HostType.BOTTOM)
            .then(closeDebuggerAndFinish);
  }
  else {
    info("switching to toolbox window.")
    gToolbox.switchHost(devtools.Toolbox.HostType.WINDOW).then(focusMainWindow).then(null, console.error);
  }
}

registerCleanupFunction(function() {
  Services.prefs.setCharPref("devtools.toolbox.host", devtools.Toolbox.HostType.BOTTOM);
  removeTab(gTab);
  removeTab(gTab2);
  gTab = null;
  gTab2 = null;
  gDebugger = null;
  gToolbox = null;
  gToolboxTab = null;
  gFocusedWindow = null;
});
