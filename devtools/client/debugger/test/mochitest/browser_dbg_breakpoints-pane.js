/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 723071: Test adding a pane to display the list of breakpoints across
 * all sources in the debuggee.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

function test() {
  let options = {
    source: "-01.js",
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gEditor = gDebugger.DebuggerView.editor;
    const gSources = gDebugger.DebuggerView.Sources;
    const queries = gDebugger.require("./content/queries");
    const actions = bindActionCreators(gPanel);
    const getState = gDebugger.DebuggerController.getState;
    const { getBreakpoint } = queries;

    let breakpointsAdded = 0;
    let breakpointsDisabled = 0;
    let breakpointsRemoved = 0;
    let breakpointsList;

    const addBreakpoints = Task.async(function* (aIncrementFlag) {
      const loc1 = { actor: gSources.selectedValue, line: 6 };
      yield actions.addBreakpoint(loc1);
      onBreakpointAdd(getBreakpoint(getState(), loc1), {
        increment: aIncrementFlag,
        line: 6,
        text: "debugger;"
      });

      const loc2 = { actor: gSources.selectedValue, line: 7 };
      yield actions.addBreakpoint(loc2);
      onBreakpointAdd(getBreakpoint(getState(), loc2), {
        increment: aIncrementFlag,
        line: 7,
        text: "function foo() {}"
      });

      const loc3 = {actor: gSources.selectedValue, line: 9 };
      yield actions.addBreakpoint(loc3);
      onBreakpointAdd(getBreakpoint(getState(), loc3), {
        increment: aIncrementFlag,
        line: 9,
        text: "foo();"
      });
    });

    function disableBreakpoints() {
      let deferred = promise.defer();

      let nodes = breakpointsList.querySelectorAll(".dbg-breakpoint");
      info("Nodes to disable: " + breakpointsAdded.length);

      is(nodes.length, breakpointsAdded,
         "The number of nodes to disable is incorrect.");

      for (let node of nodes) {
        info("Disabling breakpoint: " + node.id);

        let sourceItem = gSources.getItemForElement(node);
        let breakpointItem = gSources.getItemForElement.call(sourceItem, node);
        info("Found data: " + breakpointItem.attachment.toSource());

        actions.disableBreakpoint(breakpointItem.attachment).then(() => {
          if (++breakpointsDisabled == breakpointsAdded) {
            deferred.resolve();
          }
        });
      }

      return deferred.promise;
    }

    function removeBreakpoints() {
      let deferred = promise.defer();

      let nodes = breakpointsList.querySelectorAll(".dbg-breakpoint");
      info("Nodes to remove: " + breakpointsAdded.length);

      is(nodes.length, breakpointsAdded,
         "The number of nodes to remove is incorrect.");

      for (let node of nodes) {
        info("Removing breakpoint: " + node.id);

        let sourceItem = gSources.getItemForElement(node);
        let breakpointItem = gSources.getItemForElement.call(sourceItem, node);
        info("Found data: " + breakpointItem.attachment.toSource());

        actions.removeBreakpoint(breakpointItem.attachment).then(() => {
          if (++breakpointsRemoved == breakpointsAdded) {
            deferred.resolve();
          }
        });
      }

      return deferred.promise;
    }

    function onBreakpointAdd(bp, testData) {
      if (testData.increment) {
        breakpointsAdded++;
      }

      is(breakpointsList.querySelectorAll(".dbg-breakpoint").length, breakpointsAdded,
         testData.increment
         ? "Should have added a breakpoint in the pane."
         : "Should have the same number of breakpoints in the pane.");

      let identifier = queries.makeLocationId(bp.location);
      let node = gDebugger.document.getElementById("breakpoint-" + identifier);
      let line = node.getElementsByClassName("dbg-breakpoint-line")[0];
      let text = node.getElementsByClassName("dbg-breakpoint-text")[0];
      let check = node.querySelector("checkbox");

      ok(node,
         "Breakpoint element found successfully.");
      is(line.getAttribute("value"), testData.line,
         "The expected information wasn't found in the breakpoint element.");
      is(text.getAttribute("value"), testData.text,
         "The expected line text wasn't found in the breakpoint element.");
      is(check.getAttribute("checked"), "true",
         "The breakpoint enable checkbox is checked as expected.");
    }

    Task.spawn(function* () {
      yield waitForSourceAndCaretAndScopes(gPanel, "-02.js", 1);

      is(gDebugger.gThreadClient.state, "paused",
         "Should only be getting stack frames while paused.");
      is(queries.getSourceCount(getState()), 2,
         "Found the expected number of sources.");
      is(gEditor.getText().indexOf("debugger"), 166,
         "The correct source was loaded initially.");
      is(gSources.selectedValue, gSources.values[1],
         "The correct source is selected.");

      is(queries.getBreakpoints(getState()).length, 0,
         "No breakpoints currently added.");

      let breakpointsParent = gSources.widget._parent;
      breakpointsList = gSources.widget._list;

      is(breakpointsParent.childNodes.length, 1, // one sources list
         "Found junk in the breakpoints container.");
      is(breakpointsList.childNodes.length, 1, // one sources group
         "Found junk in the breakpoints container.");
      is(breakpointsList.querySelectorAll(".dbg-breakpoint").length, 0,
         "No breakpoints should be visible at this point.");

      yield addBreakpoints(true);

      is(breakpointsAdded, 3,
        "Should have added 3 breakpoints so far.");
      is(breakpointsDisabled, 0,
        "Shouldn't have disabled anything so far.");
      is(breakpointsRemoved, 0,
        "Shouldn't have removed anything so far.");

      is(breakpointsParent.childNodes.length, 1, // one sources list
        "Found junk in the breakpoints container.");
      is(breakpointsList.childNodes.length, 1, // one sources group
        "Found junk in the breakpoints container.");
      is(breakpointsList.querySelectorAll(".dbg-breakpoint").length, 3,
         "3 breakpoints should be visible at this point.");

      yield disableBreakpoints();

      is(breakpointsAdded, 3,
         "Should still have 3 breakpoints added so far.");
      is(breakpointsDisabled, 3,
         "Should have 3 disabled breakpoints.");
      is(breakpointsRemoved, 0,
         "Shouldn't have removed anything so far.");

      is(breakpointsParent.childNodes.length, 1, // one sources list
         "Found junk in the breakpoints container.");
      is(breakpointsList.childNodes.length, 1, // one sources group
         "Found junk in the breakpoints container.");
      is(breakpointsList.querySelectorAll(".dbg-breakpoint").length, breakpointsAdded,
         "Should have the same number of breakpoints in the pane.");
      is(breakpointsList.querySelectorAll(".dbg-breakpoint").length, breakpointsDisabled,
         "Should have the same number of disabled breakpoints.");

      yield addBreakpoints();

      is(breakpointsAdded, 3,
         "Should still have only 3 breakpoints added so far.");
      is(breakpointsDisabled, 3,
         "Should still have 3 disabled breakpoints.");
      is(breakpointsRemoved, 0,
         "Shouldn't have removed anything so far.");

      is(breakpointsParent.childNodes.length, 1, // one sources list
         "Found junk in the breakpoints container.");
      is(breakpointsList.childNodes.length, 1, // one sources group
         "Found junk in the breakpoints container.");
      is(breakpointsList.querySelectorAll(".dbg-breakpoint").length, breakpointsAdded,
         "Since half of the breakpoints already existed, but disabled, " +
         "only half of the added breakpoints are actually in the pane.");

      yield removeBreakpoints();

      is(breakpointsRemoved, 3,
         "Should have 3 removed breakpoints.");

      is(breakpointsParent.childNodes.length, 1, // one sources list
         "Found junk in the breakpoints container.");
      is(breakpointsList.childNodes.length, 1, // one sources group
         "Found junk in the breakpoints container.");
      is(breakpointsList.querySelectorAll(".dbg-breakpoint").length, 0,
         "No breakpoints should be visible at this point.");

      const cleared = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.AFTER_FRAMES_CLEARED);
      gDebugger.gThreadClient.resume();
      yield cleared;

      is(queries.getBreakpoints(getState()).length, 0,
         "No breakpoints currently added.");

      closeDebuggerAndFinish(gPanel);
    });

    callInTab(gTab, "firstCall");
  });
}
