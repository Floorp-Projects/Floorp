/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the toolbox is raised when the debugger gets paused.
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";

let gTab, gDebuggee, gPanel, gDebugger;
let gNewTab, gFocusedWindow, gToolbox, gToolboxTab;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gToolbox = gPanel._toolbox;
    gToolboxTab = gToolbox.doc.getElementById("toolbox-tab-jsdebugger");

    waitForSourceShown(gPanel, ".html").then(performTest);
  });
}

function performTest() {
  addTab(TAB_URL).then(aTab => {
    isnot(aTab, gTab,
      "The newly added tab is different from the debugger's tab.");
    is(gBrowser.selectedTab, aTab,
      "Debugger's tab is not the selected tab.");

    gNewTab = aTab;
    gFocusedWindow = window;
    testPause();
  });
}

function focusMainWindow() {
  // Make sure toolbox is not focused.
  window.addEventListener("focus", onFocus, true);
  info("Focusing main window.")

  // Execute soon to avoid any race conditions between toolbox and main window
  // getting focused.
  executeSoon(() => {
    window.focus();
  });
}

function onFocus() {
  window.removeEventListener("focus", onFocus, true);
  info("Main window focused.")

  gFocusedWindow = window;
  testPause();
}

function testPause() {
  is(gDebugger.gThreadClient.paused, false,
    "Should be running after starting the test.");

  is(gFocusedWindow, window,
    "Main window is the top level window before pause.");

  if (gToolbox.hostType == devtools.Toolbox.HostType.WINDOW) {
    gToolbox._host._window.onfocus = () => {
      gFocusedWindow = gToolbox._host._window;
    };
  }

  gDebugger.gThreadClient.addOneTimeListener("paused", () => {
    if (gToolbox.hostType == devtools.Toolbox.HostType.WINDOW) {
      is(gFocusedWindow, gToolbox._host._window,
         "Toolbox window is the top level window on pause.");
    } else {
      is(gBrowser.selectedTab, gTab,
        "Debugger's tab got selected.");
    }
    gToolbox.selectTool("webconsole").then(() => {
      ok(gToolboxTab.hasAttribute("highlighted") &&
         gToolboxTab.getAttribute("highlighted") == "true",
        "The highlighted class is present");
      ok(!gToolboxTab.hasAttribute("selected") ||
          gToolboxTab.getAttribute("selected") != "true",
        "The tab is not selected");
    }).then(() => gToolbox.selectTool("jsdebugger")).then(() => {
      ok(gToolboxTab.hasAttribute("highlighted") &&
         gToolboxTab.getAttribute("highlighted") == "true",
        "The highlighted class is present");
      ok(gToolboxTab.hasAttribute("selected") &&
         gToolboxTab.getAttribute("selected") == "true",
        "...and the tab is selected, so the glow will not be present.");
    }).then(testResume);
  });

  EventUtils.sendMouseEvent({ type: "mousedown" },
    gDebugger.document.getElementById("resume"),
    gDebugger);
}

function testResume() {
  gDebugger.gThreadClient.addOneTimeListener("resumed", () => {
    gToolbox.selectTool("webconsole").then(() => {
      ok(!gToolboxTab.hasAttribute("highlighted") ||
          gToolboxTab.getAttribute("highlighted") != "true",
        "The highlighted class is not present now after the resume");
      ok(!gToolboxTab.hasAttribute("selected") ||
          gToolboxTab.getAttribute("selected") != "true",
        "The tab is not selected");
    }).then(maybeEndTest);
  });

  EventUtils.sendMouseEvent({ type: "mousedown" },
    gDebugger.document.getElementById("resume"),
    gDebugger);
}

function maybeEndTest() {
  if (gToolbox.hostType == devtools.Toolbox.HostType.BOTTOM) {
    info("Switching to a toolbox window host.");
    gToolbox.switchHost(devtools.Toolbox.HostType.WINDOW).then(focusMainWindow);
  } else {
    info("Switching to main window host.");
    gToolbox.switchHost(devtools.Toolbox.HostType.BOTTOM).then(() => closeDebuggerAndFinish(gPanel));
  }
}

registerCleanupFunction(function() {
  // Revert to the default toolbox host, so that the following tests proceed
  // normally and not inside a non-default host.
  Services.prefs.setCharPref("devtools.toolbox.host", devtools.Toolbox.HostType.BOTTOM);

  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;

  removeTab(gNewTab);
  gNewTab = null;
  gFocusedWindow = null;
  gToolbox = null;
  gToolboxTab = null;
});
