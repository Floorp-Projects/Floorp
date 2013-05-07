/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that reloading a page with a breakpoint set does not cause it to
 * fire more than once.
 */

const TAB_URL = EXAMPLE_URL + "test-location-changes-bp.html";
const SCRIPT_URL = EXAMPLE_URL + "test-location-changes-bp.js";

var gPane = null;
var gTab = null;
var gDebuggee = null;
var gDebugger = null;
var sourcesShown = false;
var tabNavigated = false;

function test()
{
  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.panelWin;

    testAddBreakpoint();
  });
}

function testAddBreakpoint()
{
  let controller = gDebugger.DebuggerController;
  controller.activeThread.addOneTimeListener("framesadded", function() {
    Services.tm.currentThread.dispatch({ run: function() {

      var frames = gDebugger.DebuggerView.StackFrames._container._list;

      is(controller.activeThread.state, "paused",
         "The debugger statement was reached.");

      is(frames.querySelectorAll(".dbg-stackframe").length, 1,
         "Should have one frame.");

      gPane.addBreakpoint({ url: SCRIPT_URL, line: 5 }, testResume);
    }}, 0);
  });

  gDebuggee.runDebuggerStatement();
}

function testResume()
{
  is(gDebugger.DebuggerController.activeThread.state, "paused",
    "The breakpoint wasn't hit yet.");

  let thread = gDebugger.DebuggerController.activeThread;
  thread.addOneTimeListener("resumed", function() {
    thread.addOneTimeListener("paused", function() {
      executeSoon(testBreakpointHit);
    });

    EventUtils.sendMouseEvent({ type: "click" },
      content.document.querySelector("button"));
  });

  thread.resume();
}

function testBreakpointHit()
{
  is(gDebugger.DebuggerController.activeThread.state, "paused",
    "The breakpoint was hit.");

  let thread = gDebugger.DebuggerController.activeThread;
  thread.addOneTimeListener("paused", function test(aEvent, aPacket) {
    thread.addOneTimeListener("resumed", function() {
      executeSoon(testReloadPage);
    });

    is(aPacket.why.type, "debuggerStatement", "Execution has advanced to the next line.");
    isnot(aPacket.why.type, "breakpoint", "No ghost breakpoint was hit.");
    thread.resume();
  });

  thread.resume();
}

function testReloadPage()
{
  let controller = gDebugger.DebuggerController;
  controller._target.once("navigate", function onTabNavigated(aEvent, aPacket) {
    tabNavigated = true;
    ok(true, "tabNavigated event was fired.");
    info("Still attached to the tab.");
    clickAgain();
  });

  gDebugger.addEventListener("Debugger:SourceShown", function onSourcesShown() {
    sourcesShown = true;
    gDebugger.removeEventListener("Debugger:SourceShown", onSourcesShown);
    clickAgain();
  });

  content.location.reload();
}

function clickAgain()
{
  if (!sourcesShown || !tabNavigated) {
    return;
  }

  let controller = gDebugger.DebuggerController;
  controller.activeThread.addOneTimeListener("framesadded", function() {
    is(gDebugger.DebuggerController.activeThread.state, "paused",
      "The breakpoint was hit.");

    let thread = gDebugger.DebuggerController.activeThread;
    thread.addOneTimeListener("paused", function test(aEvent, aPacket) {
      thread.addOneTimeListener("resumed", function() {
        executeSoon(closeDebuggerAndFinish);
      });

      is(aPacket.why.type, "debuggerStatement", "Execution has advanced to the next line.");
      isnot(aPacket.why.type, "breakpoint", "No ghost breakpoint was hit.");
      thread.resume();
    });

    thread.resume();
  });

  EventUtils.sendMouseEvent({ type: "click" },
    content.document.querySelector("button"));
}

function testBreakpointHitAfterReload()
{
  is(gDebugger.DebuggerController.activeThread.state, "paused",
    "The breakpoint was hit.");

  let thread = gDebugger.DebuggerController.activeThread;
  thread.addOneTimeListener("paused", function test(aEvent, aPacket) {
    thread.addOneTimeListener("resumed", function() {
      executeSoon(closeDebuggerAndFinish);
    });

    is(aPacket.why.type, "debuggerStatement", "Execution has advanced to the next line.");
    isnot(aPacket.why.type, "breakpoint", "No ghost breakpoint was hit.");
    thread.resume();
  });

  thread.resume();
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebuggee = null;
  gDebugger = null;
});
