/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 723071: Test adding a pane to display the list of breakpoints across
 * all sources in the debuggee.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

function test() {
  let gTab, gPanel, gDebugger;
  let gEditor, gSources, gBreakpoints, gBreakpointsAdded, gBreakpointsRemoving;

  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gBreakpoints = gDebugger.DebuggerController.Breakpoints;
    gBreakpointsAdded = gBreakpoints._added;
    gBreakpointsRemoving = gBreakpoints._removing;

    waitForSourceAndCaretAndScopes(gPanel, "-02.js", 1).then(performTest);
    callInTab(gTab, "firstCall");
  });

  let breakpointsAdded = 0;
  let breakpointsDisabled = 0;
  let breakpointsRemoved = 0;

  function performTest() {
    is(gDebugger.gThreadClient.state, "paused",
      "Should only be getting stack frames while paused.");
    is(gSources.itemCount, 2,
      "Found the expected number of sources.");
    is(gEditor.getText().indexOf("debugger"), 166,
      "The correct source was loaded initially.");
    is(gSources.selectedValue, gSources.values[1],
      "The correct source is selected.");

    is(gBreakpointsAdded.size, 0,
      "No breakpoints currently added.");
    is(gBreakpointsRemoving.size, 0,
      "No breakpoints currently being removed.");
    is(gEditor.getBreakpoints().length, 0,
      "No breakpoints currently shown in the editor.");

    ok(!gBreakpoints._getAdded({ url: "foo", line: 3 }),
      "_getAdded('foo', 3) returns falsey.");
    ok(!gBreakpoints._getRemoving({ url: "bar", line: 3 }),
      "_getRemoving('bar', 3) returns falsey.");

    let breakpointsParent = gSources.widget._parent;
    let breakpointsList = gSources.widget._list;

    is(breakpointsParent.childNodes.length, 1, // one sources list
      "Found junk in the breakpoints container.");
    is(breakpointsList.childNodes.length, 1, // one sources group
      "Found junk in the breakpoints container.");
    is(breakpointsList.querySelectorAll(".dbg-breakpoint").length, 0,
      "No breakpoints should be visible at this point.");

    addBreakpoints(true).then(() => {
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

      disableBreakpoints().then(() => {
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

        addBreakpoints().then(() => {
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

          removeBreakpoints().then(() => {
            is(breakpointsRemoved, 3,
              "Should have 3 removed breakpoints.");

            is(breakpointsParent.childNodes.length, 1, // one sources list
               "Found junk in the breakpoints container.");
            is(breakpointsList.childNodes.length, 1, // one sources group
               "Found junk in the breakpoints container.");
            is(breakpointsList.querySelectorAll(".dbg-breakpoint").length, 0,
               "No breakpoints should be visible at this point.");

            waitForDebuggerEvents(gPanel, gDebugger.EVENTS.AFTER_FRAMES_CLEARED).then(() => {
              finalCheck();
              closeDebuggerAndFinish(gPanel);
            });

            gDebugger.gThreadClient.resume();
          });
        });
      });
    });

    function addBreakpoints(aIncrementFlag) {
      let deferred = promise.defer();

      gPanel.addBreakpoint({ actor: gSources.selectedValue, line: 6 }).then(aClient => {
        onBreakpointAdd(aClient, {
          increment: aIncrementFlag,
          line: 6,
          text: "debugger;"
        });

        gPanel.addBreakpoint({ actor: gSources.selectedValue, line: 7 }).then(aClient => {
          onBreakpointAdd(aClient, {
            increment: aIncrementFlag,
            line: 7,
            text: "function foo() {}"
          });

          gPanel.addBreakpoint({ actor: gSources.selectedValue, line: 9 }).then(aClient => {
            onBreakpointAdd(aClient, {
              increment: aIncrementFlag,
              line: 9,
              text: "foo();"
            });

            deferred.resolve();
          });
        });
      });

      return deferred.promise;
    }

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

        gSources.disableBreakpoint(breakpointItem.attachment).then(() => {
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

        gPanel.removeBreakpoint(breakpointItem.attachment).then(() => {
          if (++breakpointsRemoved == breakpointsAdded) {
            deferred.resolve();
          }
        });
      }

      return deferred.promise;
    }

    function onBreakpointAdd(aBreakpointClient, aTestData) {
      if (aTestData.increment) {
        breakpointsAdded++;
      }

      is(breakpointsList.querySelectorAll(".dbg-breakpoint").length, breakpointsAdded,
        aTestData.increment
          ? "Should have added a breakpoint in the pane."
          : "Should have the same number of breakpoints in the pane.");

      let identifier = gBreakpoints.getIdentifier(aBreakpointClient.location);
      let node = gDebugger.document.getElementById("breakpoint-" + identifier);
      let line = node.getElementsByClassName("dbg-breakpoint-line")[0];
      let text = node.getElementsByClassName("dbg-breakpoint-text")[0];
      let check = node.querySelector("checkbox");

      ok(node,
        "Breakpoint element found successfully.");
      is(line.getAttribute("value"), aTestData.line,
        "The expected information wasn't found in the breakpoint element.");
      is(text.getAttribute("value"), aTestData.text,
        "The expected line text wasn't found in the breakpoint element.");
      is(check.getAttribute("checked"), "true",
        "The breakpoint enable checkbox is checked as expected.");
    }
  }

  function finalCheck() {
    is(gBreakpointsAdded.size, 0,
      "No breakpoints currently added.");
    is(gBreakpointsRemoving.size, 0,
      "No breakpoints currently being removed.");
    is(gEditor.getBreakpoints().length, 0,
      "No breakpoints currently shown in the editor.");
  }
}
