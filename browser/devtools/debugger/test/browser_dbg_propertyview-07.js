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
    gDebugger = gPane.debuggerWindow;

    testFrameParameters();
  });
}

function testFrameParameters()
{
  // scriptsadded is fired last when switching to a paused state, so the
  // property view will have had a chance to fetch the call parameters.
  gPane.activeThread.addOneTimeListener("scriptsadded", function() {
    Services.tm.currentThread.dispatch({ run: function() {

      var frames = gDebugger.DebuggerView.Stackframes._frames,
          childNodes = frames.childNodes,
          localScope = gDebugger.DebuggerView.Properties.localScope,
          localNodes = localScope.querySelector(".details").childNodes;

      is(gDebugger.StackFrames.activeThread.state, "paused",
        "Should only be getting stack frames while paused.");

      is(frames.querySelectorAll(".dbg-stackframe").length, 3,
        "Should have three frames.");

      is(localNodes.length, 8,
        "The localScope should contain all the created variable elements.");

      is(localNodes[0].querySelector(".info").textContent, "[object Proxy]",
        "Should have the right property value for 'this'.");

      is(localNodes[1].querySelector(".info").textContent, "[object Arguments]",
        "Should have the right property value for 'arguments'.");

      is(localNodes[2].querySelector(".info").textContent, "[object Object]",
        "Should have the right property value for 'aArg'.");

      is(localNodes[3].querySelector(".info").textContent, '"beta"',
        "Should have the right property value for 'bArg'.");

      is(localNodes[4].querySelector(".info").textContent, "3",
        "Should have the right property value for 'cArg'.");

      is(localNodes[5].querySelector(".info").textContent, "false",
        "Should have the right property value for 'dArg'.");

      is(localNodes[6].querySelector(".info").textContent, "null",
        "Should have the right property value for 'eArg'.");

      is(localNodes[7].querySelector(".info").textContent, "undefined",
        "Should have the right property value for 'fArg'.");

      resumeAndFinish();
    }}, 0);
  });

  EventUtils.sendMouseEvent({ type: "click" },
    content.document.querySelector("button"),
    content.window);
}

function resumeAndFinish() {
  gPane.activeThread.addOneTimeListener("framescleared", function() {
    Services.tm.currentThread.dispatch({ run: function() {
      var frames = gDebugger.DebuggerView.Stackframes._frames;

      is(frames.querySelectorAll(".dbg-stackframe").length, 0,
        "Should have no frames.");

      closeDebuggerAndFinish(gTab);
    }}, 0);
  });

  gDebugger.StackFrames.activeThread.resume();
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebugger = null;
});
