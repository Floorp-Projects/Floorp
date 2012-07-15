/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 723071: test adding a pane to display the list of breakpoints across
 * all scripts in the debuggee.
 */

const TAB_URL = EXAMPLE_URL + "browser_dbg_script-switching.html";

let gPane = null;
let gTab = null;
let gDebuggee = null;
let gDebugger = null;
let gScripts = null;
let gBreakpoints = null;
let gBreakpointsElement = null;

function test()
{
  let scriptShown = false;
  let framesAdded = false;
  let resumed = false;
  let testStarted = false;

  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.contentWindow;
    resumed = true;

    gDebugger.DebuggerController.activeThread.addOneTimeListener("framesadded", function() {
      framesAdded = true;
      executeSoon(startTest);
    });

    executeSoon(function() {
      gDebuggee.firstCall();
    });
  });

  function onScriptShown(aEvent)
  {
    scriptShown = aEvent.detail.url.indexOf("-02.js") != -1;
    executeSoon(startTest);
  }

  window.addEventListener("Debugger:ScriptShown", onScriptShown);

  function startTest()
  {
    if (scriptShown && framesAdded && resumed && !testStarted) {
      window.removeEventListener("Debugger:ScriptShown", onScriptShown);
      testStarted = true;
      Services.tm.currentThread.dispatch({ run: performTest }, 0);
    }
  }

  let breakpointsAdded = 0;
  let breakpointsDisabled = 0;
  let breakpointsRemoved = 0;

  function performTest()
  {
    gScripts = gDebugger.DebuggerView.Scripts;

    is(gDebugger.DebuggerController.activeThread.state, "paused",
      "Should only be getting stack frames while paused.");

    is(gScripts._scripts.itemCount, 2, "Found the expected number of scripts.");

    let editor = gDebugger.editor;

    isnot(editor.getText().indexOf("debugger"), -1,
          "The correct script was loaded initially.");
    isnot(gScripts.selected, gScripts.scriptLocations[0],
          "the correct script is selected");

    gBreakpoints = gPane.breakpoints;
    is(Object.keys(gBreakpoints), 0, "no breakpoints");
    ok(!gPane.getBreakpoint("chocolate", 3), "getBreakpoint('chocolate', 3) returns falsey");

    is(editor.getBreakpoints().length, 0, "no breakpoints in the editor");

    gBreakpointsElement = gDebugger.DebuggerView.Breakpoints._breakpoints;
    is(gBreakpointsElement.childNodes.length, 1,
      "The breakpoints pane should be empty, but showing a " +
      "'no breakpoints' information message.");
    is(gBreakpointsElement.childNodes.length,
       gBreakpointsElement.querySelectorAll(".list-item.empty").length,
       "Found junk in the breakpoints container.");

    addBreakpoints(function() {
      is(breakpointsAdded, 3,
        "Should have added 3 breakpoints so far.");
      is(breakpointsDisabled, 0,
        "Shouldn't have disabled anything so far.");
      is(breakpointsRemoved, 0,
        "Shouldn't have removed anything so far.");

      is(gBreakpointsElement.childNodes.length,
         gBreakpointsElement.querySelectorAll(".dbg-breakpoint").length,
         "Found junk in the breakpoints container.");

      disableBreakpoints(function() {
        is(breakpointsAdded, 3,
          "Should still have 3 breakpoints added so far.");
        is(breakpointsDisabled, 3,
          "Should have 3 disabled breakpoints.");
        is(breakpointsRemoved, 0,
          "Shouldn't have removed anything so far.");

        is(gBreakpointsElement.childNodes.length, breakpointsAdded,
          "Should have the same number of breakpoints in the pane.");
        is(gBreakpointsElement.childNodes.length, breakpointsDisabled,
          "Should have the same number of disabled breakpoints.");

        addBreakpoints(function() {
          is(breakpointsAdded, 3,
            "Should still have only 3 breakpoints added so far.");
          is(breakpointsDisabled, 3,
            "Should still have 3 disabled breakpoints.");
          is(breakpointsRemoved, 0,
            "Shouldn't have removed anything so far.");

          is(gBreakpointsElement.childNodes.length, breakpointsAdded,
            "Since half of the breakpoints already existed, but disabled, " +
            "only half of the added breakpoints are actually in the pane.");
          is(gBreakpointsElement.childNodes.length,
             gBreakpointsElement.querySelectorAll(".dbg-breakpoint").length,
             "Found junk in the breakpoints container.");

          removeBreakpoints(function() {
            is(breakpointsRemoved, 3,
              "Should have 3 removed breakpoints.");

            is(gBreakpointsElement.childNodes.length, 1,
              "The breakpoints pane should be empty, but showing a " +
              "'no breakpoints' information message.");
            is(gBreakpointsElement.childNodes.length,
               gBreakpointsElement.querySelectorAll(".list-item.empty").length,
               "Found junk in the breakpoints container.");

            finish();
          });
        });
      });
    }, true);

    function addBreakpoints(callback, increment)
    {
      let line;

      executeSoon(function()
      {
        line = 4;
        gPane.addBreakpoint({url: gScripts.selected, line: line},
          function(cl, err) {
          onBreakpointAdd.call({ increment: increment, line: line }, cl, err);

          line = 5;
          gPane.addBreakpoint({url: gScripts.selected, line: line},
            function(cl, err) {
            onBreakpointAdd.call({ increment: increment, line: line }, cl, err);

            line = 6;
            gPane.addBreakpoint({url: gScripts.selected, line: line},
              function(cl, err) {
              onBreakpointAdd.call({ increment: increment, line: line }, cl, err);

              executeSoon(function() {
                callback();
              });
            });
          });
        });
      });
    }

    function disableBreakpoints(callback)
    {
      let nodes = Array.slice(gBreakpointsElement.childNodes);
      info("Nodes to disable: " + breakpointsAdded);
      is(nodes.length, breakpointsAdded,
        "The number of nodes to disable is incorrect.");

      Array.forEach(nodes, function(bkp) {
        info("Disabling breakpoint: " + bkp.id);

        gDebugger.DebuggerView.Breakpoints.disableBreakpoint(bkp, function() {
          if (++breakpointsDisabled !== breakpointsAdded) {
            return;
          }
          executeSoon(function() {
            callback();
          });
        });
      });
    }

    function removeBreakpoints(callback)
    {
      let nodes = Array.slice(gBreakpointsElement.childNodes);
      info("Nodes to remove: " + breakpointsAdded);
      is(nodes.length, breakpointsAdded,
        "The number of nodes to remove is incorrect.");

      Array.forEach(nodes, function(bkp) {
        info("Removing breakpoint: " + bkp.id);

        let [url, line, actor] =
          [bkp.breakpointUrl, bkp.breakpointLine, bkp.breakpointActor];

        gDebugger.DebuggerView.Breakpoints.removeBreakpoint(actor);
        gPane.removeBreakpoint(gPane.getBreakpoint(url, line), function() {
          if (++breakpointsRemoved !== breakpointsAdded) {
            return;
          }
          executeSoon(function() {
            callback();
          });
        });
      });
    }

    function onBreakpointAdd(aBreakpointClient, aResponseError)
    {
      if (this.increment) {
        breakpointsAdded++;
      }

      is(gBreakpointsElement.childNodes.length, breakpointsAdded, this.increment
        ? "Should have added a breakpoint in the pane."
        : "Should have the same number of breakpoints in the pane.");

      let id = "breakpoint-" + aBreakpointClient.actor;
      let bkp = gDebugger.document.getElementById(id);
      let info = bkp.getElementsByClassName("dbg-breakpoint-info")[0];
      let text = bkp.getElementsByClassName("dbg-breakpoint-text")[0];
      let check = bkp.querySelector("checkbox");

      is(bkp.id, id,
        "Breakpoint element " + id + " found succesfully.");
      is(info.getAttribute("value"), getExpectedBreakpointInfo(this.line),
        "The expected information wasn't found in the breakpoint element.");
      is(text.getAttribute("value"), getExpectedLineText(this.line).trim(),
        "The expected line text wasn't found in the breakpoint element.");
      is(check.getAttribute("checked"), "true",
        "The breakpoint enable checkbox is checked as expected.");
    }

    function getExpectedBreakpointInfo(line) {
      let url = gDebugger.DebuggerView.Scripts.selected;
      let label = gDebugger.DebuggerController.SourceScripts.getScriptLabel(url);
      return label + ":" + line;
    }

    function getExpectedLineText(line) {
      return gDebugger.DebuggerController.SourceScripts.getLineText(line - 1);
    }
  }

  registerCleanupFunction(function() {
    is(Object.keys(gBreakpoints).length, 0, "no breakpoint in the debugger");
    ok(!gPane.getBreakpoint(gScripts.scriptLocations[0], 5),
       "getBreakpoint(scriptLocations[0], 5) returns no breakpoint");

    is(breakpointsAdded, 3, "correct number of breakpoints have been added");
    is(breakpointsDisabled, 3, "correct number of breakpoints have been disabled");
    is(breakpointsRemoved, 3, "correct number of breakpoints have been removed");
    removeTab(gTab);
    gPane = null;
    gTab = null;
    gDebuggee = null;
    gDebugger = null;
    gScripts = null;
    gBreakpoints = null;
    gBreakpointsElement = null;
  });
}
