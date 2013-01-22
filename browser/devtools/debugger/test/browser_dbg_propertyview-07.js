/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the property view displays function parameters.
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
    gDebugger = gPane.panelWin;

    testFrameParameters();
  });
}

function testFrameParameters()
{
  gDebugger.addEventListener("Debugger:FetchedVariables", function test() {
    gDebugger.removeEventListener("Debugger:FetchedVariables", test, false);
    Services.tm.currentThread.dispatch({ run: function() {

      var frames = gDebugger.DebuggerView.StackFrames._container._list,
          childNodes = frames.childNodes,
          localScope = gDebugger.DebuggerView.Variables._list.querySelector(".scope"),
          localNodes = localScope.querySelector(".details").childNodes;

      is(gDebugger.DebuggerController.activeThread.state, "paused",
        "Should only be getting stack frames while paused.");

      is(frames.querySelectorAll(".dbg-stackframe").length, 3,
        "Should have three frames.");

      is(localNodes.length, 11,
        "The localScope should contain all the created variable elements.");

      is(localNodes[0].querySelector(".value").getAttribute("value"), "[object Proxy]",
        "Should have the right property value for 'this'.");

      is(localNodes[1].querySelector(".value").getAttribute("value"), "[object Object]",
        "Should have the right property value for 'aArg'.");

      is(localNodes[2].querySelector(".value").getAttribute("value"), '"beta"',
        "Should have the right property value for 'bArg'.");

      is(localNodes[3].querySelector(".value").getAttribute("value"), "3",
        "Should have the right property value for 'cArg'.");

      is(localNodes[4].querySelector(".value").getAttribute("value"), "false",
        "Should have the right property value for 'dArg'.");

      is(localNodes[5].querySelector(".value").getAttribute("value"), "null",
        "Should have the right property value for 'eArg'.");

      is(localNodes[6].querySelector(".value").getAttribute("value"), "undefined",
        "Should have the right property value for 'fArg'.");

      is(localNodes[7].querySelector(".value").getAttribute("value"), "1",
       "Should have the right property value for 'a'.");

      is(localNodes[8].querySelector(".value").getAttribute("value"), "[object Arguments]",
        "Should have the right property value for 'arguments'.");

      is(localNodes[9].querySelector(".value").getAttribute("value"), "[object Object]",
       "Should have the right property value for 'b'.");

      is(localNodes[10].querySelector(".value").getAttribute("value"), "[object Object]",
       "Should have the right property value for 'c'.");

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

    var frames = gDebugger.DebuggerView.StackFrames._container._list;
    is(frames.querySelectorAll(".dbg-stackframe").length, 0,
      "Should have no frames.");

    closeDebuggerAndFinish();
  }, true);

  gDebugger.DebuggerController.activeThread.resume();
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebugger = null;
});
