/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that debugger's tab is highlighted when it is paused and not the
// currently selected tool.

var gTab = null;
var gDebugger = null;
var gToolbox = null;
var gToolboxTab = null;

function test() {
  debug_tab_pane(STACK_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebugger = aPane.panelWin;
    gToolbox = aPane._toolbox;
    gToolboxTab = gToolbox.doc.getElementById("toolbox-tab-jsdebugger");
    testPause();
  });
}

function testPause() {
  is(gDebugger.DebuggerController.activeThread.paused, false,
    "Should be running after debug_tab_pane.");

  gDebugger.DebuggerController.activeThread.addOneTimeListener("paused", function() {
    Services.tm.currentThread.dispatch({ run: function() {

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
      }).then(closeDebuggerAndFinish);
    }}, 0);
  });

  EventUtils.sendMouseEvent({ type: "mousedown" },
    gDebugger.document.getElementById("resume"),
    gDebugger);
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gTab = null;
  gDebugger = null;
  gToolbox = null;
  gToolboxTab = null;
});
