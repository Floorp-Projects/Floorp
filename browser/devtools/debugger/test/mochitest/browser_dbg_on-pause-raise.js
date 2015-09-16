/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the toolbox is raised when the debugger gets paused.
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";

var gTab, gPanel, gDebugger;
var gFocusedWindow, gToolbox, gToolboxTab;

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
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

  if (gToolbox.hostType == Toolbox.HostType.WINDOW) {
    gToolbox._host._window.addEventListener("focus", function onFocus() {
      gToolbox._host._window.removeEventListener("focus", onFocus, true);
      gFocusedWindow = gToolbox._host._window;
    }, true);
  }

  gDebugger.gThreadClient.addOneTimeListener("paused", () => {
    if (gToolbox.hostType == Toolbox.HostType.WINDOW) {
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

  // Evaluate a script to fully pause the debugger
  once(gDebugger.gClient, "willInterrupt").then(() => {
    evalInTab(gTab, "1+1;");
  });
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
  if (gToolbox.hostType == Toolbox.HostType.BOTTOM) {
    info("Switching to a toolbox window host.");
    gToolbox.switchHost(Toolbox.HostType.WINDOW).then(focusMainWindow);
  } else {
    info("Switching to main window host.");
    gToolbox.switchHost(Toolbox.HostType.BOTTOM).then(() => closeDebuggerAndFinish(gPanel));
  }
}

registerCleanupFunction(function() {
  // Revert to the default toolbox host, so that the following tests proceed
  // normally and not inside a non-default host.
  Services.prefs.setCharPref("devtools.toolbox.host", Toolbox.HostType.BOTTOM);

  gTab = null;
  gPanel = null;
  gDebugger = null;

  gFocusedWindow = null;
  gToolbox = null;
  gToolboxTab = null;
});
