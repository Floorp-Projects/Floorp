/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 771452: make sure that setting a breakpoint in an inline script doesn't
// add it twice.

const TAB_URL = EXAMPLE_URL + "browser_dbg_breakpoint-new-script.html";

var gPane = null;
var gTab = null;
var gDebugger = null;
var gDebuggee = null;

function test()
{
  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gPane = aPane;
    gDebugger = gPane.contentWindow;
    gDebuggee = aDebuggee;

    testAddBreakpoint();
  });
}

function testAddBreakpoint()
{
  gDebugger.addEventListener("Debugger:FetchedVariables", function test() {
    gDebugger.removeEventListener("Debugger:FetchedVariables", test, false);
    executeSoon(function() {
      var frames = gDebugger.DebuggerView.StackFrames._frames;

      is(gDebugger.DebuggerController.activeThread.state, "paused",
         "The debugger statement was reached.");

      is(frames.querySelectorAll(".dbg-stackframe").length, 1,
         "Should have one frame.");

      let location = { url: TAB_URL, line: 9 };
      gPane.addBreakpoint(location, function (aResponse, bpClient) {
        testResume();
      });
    });
  }, false);

  gDebuggee.runDebuggerStatement();
}

function testResume()
{
  gDebugger.DebuggerController.activeThread.addOneTimeListener("resumed", function test() {
    gDebugger.DebuggerController.activeThread.addOneTimeListener("paused", function test() {
      executeSoon(testBreakpointHit);
    }, false);

    EventUtils.sendMouseEvent({ type: "click" },
      content.document.querySelector("button"),
      content.window);

  });

  gDebugger.DebuggerController.activeThread.resume();
}

function testBreakpointHit()
{
  var frames = gDebugger.DebuggerView.StackFrames._frames;

  is(gDebugger.DebuggerController.activeThread.state, "paused",
    "The breakpoint was hit.");

  resumeAndFinish();
}

function resumeAndFinish() {
  let thread = gDebugger.DebuggerController.activeThread;
  thread.addOneTimeListener("paused", function test(aEvent, aPacket) {
    is(aPacket.why.type, "debuggerStatement", "Execution has advanced to the next line.");
    isnot(aPacket.why.type, "breakpoint", "No ghost breakpoint was hit.");

    closeDebuggerAndFinish();
  });

  thread.resume();
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebugger = null;
  gDebuggee = null;
});
