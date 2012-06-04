/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the pause-on-exceptions toggle works.
 */

const TAB_URL = EXAMPLE_URL + "browser_dbg_pause-exceptions.html";

var gPane = null;
var gTab = null;
var gDebugger = null;
var gCount = 0;

function test()
{
  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gPane = aPane;
    gDebugger = gPane.contentWindow;

    testWithFrame();
  });
}

function testWithFrame()
{
  gPane.contentWindow.gClient.addOneTimeListener("paused", function() {
    gDebugger.addEventListener("Debugger:FetchedVariables", function testA() {
      // We expect 2 Debugger:FetchedVariables events, one from the global object
      // scope and the regular one.
      if (++gCount <2) {
        is(gCount, 1, "A. First Debugger:FetchedVariables event received.");
        return;
      }
      is(gCount, 2, "A. Second Debugger:FetchedVariables event received.");
      gDebugger.removeEventListener("Debugger:FetchedVariables", testA, false);

      is(gDebugger.DebuggerController.activeThread.state, "paused",
        "Should be paused now.");

      EventUtils.sendMouseEvent({ type: "click" },
        gDebugger.document.getElementById("pause-exceptions"),
        gDebugger);

      is(gDebugger.DebuggerController.StackFrames.pauseOnExceptions, true,
        "The option should be enabled now.");

      gCount = 0;
      gPane.contentWindow.gClient.addOneTimeListener("resumed", function() {
        gDebugger.addEventListener("Debugger:FetchedVariables", function testB() {
          // We expect 2 Debugger:FetchedVariables events, one from the global object
          // scope and the regular one.
          if (++gCount <2) {
            is(gCount, 1, "B. First Debugger:FetchedVariables event received.");
            return;
          }
          is(gCount, 2, "B. Second Debugger:FetchedVariables event received.");
          gDebugger.removeEventListener("Debugger:FetchedVariables", testB, false);
          Services.tm.currentThread.dispatch({ run: function() {

            var frames = gDebugger.DebuggerView.StackFrames._frames,
                scopes = gDebugger.DebuggerView.Properties._vars,
                innerScope = scopes.firstChild,
                innerNodes = innerScope.querySelector(".details").childNodes;

            is(gDebugger.DebuggerController.activeThread.state, "paused",
              "Should only be getting stack frames while paused.");

            is(frames.querySelectorAll(".dbg-stackframe").length, 1,
              "Should have one frame.");

            is(scopes.children.length, 3, "Should have 3 variable scopes.");

            is(innerNodes[0].querySelector(".name").getAttribute("value"), "<exception>",
              "Should have the right property name for the exception.");

            is(innerNodes[0].querySelector(".value").getAttribute("value"), "[object Error]",
              "Should have the right property value for the exception.");

            resumeAndFinish();
          }}, 0);
        }, false);
      });

      EventUtils.sendMouseEvent({ type: "click" },
        gDebugger.document.getElementById("resume"),
        gDebugger);
    }, false);
  });

  EventUtils.sendMouseEvent({ type: "click" },
    content.document.querySelector("button"),
    content.window);
}

function resumeAndFinish() {
  gPane.contentWindow.gClient.addOneTimeListener("resumed", function() {
    Services.tm.currentThread.dispatch({ run: function() {

      closeDebuggerAndFinish(false);
    }}, 0);
  });

  // Resume to let the exception reach it's catch clause.
  gDebugger.DebuggerController.activeThread.resume();
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebugger = null;
});
