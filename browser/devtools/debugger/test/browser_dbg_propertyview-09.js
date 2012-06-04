/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the property view populates the global scope pane.
 */

const TAB_URL = EXAMPLE_URL + "browser_dbg_frame-parameters.html";

var gPane = null;
var gTab = null;
var gDebugger = null;

function test()
{
  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gPane = aPane;
    gDebugger = gPane.contentWindow;

    testFrameParameters();
  });
}

function testFrameParameters()
{
  let count = 0;
  gDebugger.addEventListener("Debugger:FetchedVariables", function test() {
    // We expect 2 Debugger:FetchedVariables events, one from the global object
    // scope and the regular one.
    if (++count <2) {
      info("Number of received Debugger:FetchedVariables events: " + count);
      return;
    }
    gDebugger.removeEventListener("Debugger:FetchedVariables", test, false);
    Services.tm.currentThread.dispatch({ run: function() {

      var frames = gDebugger.DebuggerView.StackFrames._frames,
          globalScope = gDebugger.DebuggerView.Properties._vars.lastChild,
          globalNodes = globalScope.querySelector(".details").childNodes;

      globalScope.expand();

      is(gDebugger.DebuggerController.activeThread.state, "paused",
        "Should only be getting stack frames while paused.");

      is(frames.querySelectorAll(".dbg-stackframe").length, 3,
        "Should have three frames.");

      is(globalNodes[0].querySelector(".name").getAttribute("value"), "Array",
        "Should have the right property name for |Array|.");

      is(globalNodes[0].querySelector(".value").getAttribute("value"), "[object Function]",
        "Should have the right property value for |Array|.");

      let len = globalNodes.length - 1;
      is(globalNodes[len].querySelector(".name").getAttribute("value"), "window",
        "Should have the right property name for |window|.");

      is(globalNodes[len].querySelector(".value").getAttribute("value"), "[object Proxy]",
        "Should have the right property value for |window|.");

      resumeAndFinish();
    }}, 0);
  }, false);

  EventUtils.sendMouseEvent({ type: "click" },
    content.document.querySelector("button"),
    content.window);
}

function resumeAndFinish() {
  gDebugger.addEventListener("Debugger:AfterFramesCleared", function listener() {
    gDebugger.removeEventListener("Debugger:AfterFramesCleared", listener, true);
    Services.tm.currentThread.dispatch({ run: function() {
      var frames = gDebugger.DebuggerView.StackFrames._frames;

      is(frames.querySelectorAll(".dbg-stackframe").length, 0,
        "Should have no frames.");

      closeDebuggerAndFinish();
    }}, 0);
  }, true);

  gDebugger.DebuggerController.activeThread.resume();
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebugger = null;
});
