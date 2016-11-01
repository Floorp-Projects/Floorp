/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the toolbox is raised when the debugger gets paused.
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";

add_task(function *() {
  let options = {
    source: TAB_URL,
    line: 1
  };
  let [tab,, panel] = yield initDebugger(TAB_URL, options);
  let panelWin = panel.panelWin;
  let toolbox = panel._toolbox;
  let toolboxTab = toolbox.doc.getElementById("toolbox-tab-jsdebugger");

  let newTab = yield addTab(TAB_URL);
  isnot(newTab, tab,
    "The newly added tab is different from the debugger's tab.");
  is(gBrowser.selectedTab, newTab,
    "Debugger's tab is not the selected tab.");

  info("Run tests against bottom host.");
  yield testPause();
  yield testResume();

  // testResume selected the console, select back the debugger.
  yield toolbox.selectTool("jsdebugger");

  info("Switching to a toolbox window host.");
  yield toolbox.switchHost(Toolbox.HostType.WINDOW);

  info("Run tests against window host.");
  yield testPause();
  yield testResume();

  info("Cleanup after the test.");
  yield toolbox.switchHost(Toolbox.HostType.BOTTOM);
  yield closeDebuggerAndFinish(panel);

  function* testPause() {
    is(panelWin.gThreadClient.paused, false,
      "Should be running after starting the test.");

    let onFocus, onTabSelect;
    if (toolbox.hostType == Toolbox.HostType.WINDOW) {
      onFocus = new Promise(done => {
        toolbox.win.parent.addEventListener("focus", function onFocus() {
          toolbox.win.parent.removeEventListener("focus", onFocus, true);
          done();
        }, true);
      });
    } else {
      onTabSelect = new Promise(done => {
        tab.parentNode.addEventListener("TabSelect", function listener({type}) {
          tab.parentNode.removeEventListener(type, listener);
          done();
        });
      });
    }

    let onPaused = waitForPause(panelWin.gThreadClient);

    // Evaluate a script to fully pause the debugger
    evalInTab(tab, "debugger;");

    yield onPaused;
    yield onFocus;
    yield onTabSelect;

    if (toolbox.hostType != Toolbox.HostType.WINDOW) {
      is(gBrowser.selectedTab, tab,
        "Debugger's tab got selected.");
    }

    yield toolbox.selectTool("webconsole");
    ok(toolboxTab.hasAttribute("highlighted") &&
       toolboxTab.getAttribute("highlighted") == "true",
      "The highlighted class is present");
    ok(!toolboxTab.hasAttribute("selected") ||
        toolboxTab.getAttribute("selected") != "true",
      "The tab is not selected");
    yield toolbox.selectTool("jsdebugger");
    ok(toolboxTab.hasAttribute("highlighted") &&
       toolboxTab.getAttribute("highlighted") == "true",
      "The highlighted class is present");
    ok(toolboxTab.hasAttribute("selected") &&
       toolboxTab.getAttribute("selected") == "true",
      "...and the tab is selected, so the glow will not be present.");
  }

  function* testResume() {
    let onPaused = waitForEvent(panelWin.gThreadClient, "resumed");

    EventUtils.sendMouseEvent({ type: "mousedown" },
      panelWin.document.getElementById("resume"),
      panelWin);

    yield onPaused;

    yield toolbox.selectTool("webconsole");
    ok(!toolboxTab.hasAttribute("highlighted") ||
        toolboxTab.getAttribute("highlighted") != "true",
      "The highlighted class is not present now after the resume");
    ok(!toolboxTab.hasAttribute("selected") ||
        toolboxTab.getAttribute("selected") != "true",
      "The tab is not selected");
  }
});

registerCleanupFunction(function () {
  // Revert to the default toolbox host, so that the following tests proceed
  // normally and not inside a non-default host.
  Services.prefs.setCharPref("devtools.toolbox.host", Toolbox.HostType.BOTTOM);
});
