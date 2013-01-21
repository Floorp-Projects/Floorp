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
var gPrevPref = null;

requestLongerTimeout(2);

function test()
{
  gPrevPref = Services.prefs.getBoolPref(
    "devtools.debugger.ui.pause-on-exceptions");
  Services.prefs.setBoolPref(
    "devtools.debugger.ui.pause-on-exceptions", true);

  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gPane = aPane;
    gDebugger = gPane.panelWin;

    gDebugger.DebuggerController.StackFrames.autoScopeExpand = true;
    gDebugger.DebuggerView.Variables.nonEnumVisible = false;
    testWithFrame();
  });
}

function testWithFrame()
{
  let count = 0;
  gPane.panelWin.gClient.addOneTimeListener("paused", function() {
    gDebugger.addEventListener("Debugger:FetchedVariables", function testA() {
      // We expect 2 Debugger:FetchedVariables events, one from the global object
      // scope and the regular one.
      if (++count < 2) {
        is(count, 1, "A. First Debugger:FetchedVariables event received.");
        return;
      }
      is(count, 2, "A. Second Debugger:FetchedVariables event received.");
      gDebugger.removeEventListener("Debugger:FetchedVariables", testA, false);

      is(gDebugger.DebuggerController.activeThread.state, "paused",
        "Should be paused now.");

      // Pause on exceptions should be already enabled.
      is(gPrevPref, false,
        "The pause-on-exceptions functionality should be disabled by default.");
      is(gDebugger.Prefs.pauseOnExceptions, true,
        "The pause-on-exceptions pref should be true from startup.");
      is(gDebugger.DebuggerView.Options._pauseOnExceptionsItem.getAttribute("checked"), "true",
        "Pause on exceptions should be enabled from startup. ")

      count = 0;
      gPane.panelWin.gClient.addOneTimeListener("resumed", function() {
        gDebugger.addEventListener("Debugger:FetchedVariables", function testB() {
          // We expect 2 Debugger:FetchedVariables events, one from the global object
          // scope and the regular one.
          if (++count < 2) {
            is(count, 1, "B. First Debugger:FetchedVariables event received.");
            return;
          }
          is(count, 2, "B. Second Debugger:FetchedVariables event received.");
          gDebugger.removeEventListener("Debugger:FetchedVariables", testB, false);
          Services.tm.currentThread.dispatch({ run: function() {

            var frames = gDebugger.DebuggerView.StackFrames._container._list,
                scopes = gDebugger.DebuggerView.Variables._list,
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

            // Disable pause on exceptions.
            gDebugger.DebuggerView.Options._pauseOnExceptionsItem.setAttribute("checked", "false");
            gDebugger.DebuggerView.Options._togglePauseOnExceptions();

            is(gDebugger.Prefs.pauseOnExceptions, false,
              "The pause-on-exceptions pref should have been set to false.");

            resumeAndFinish();

          }}, 0);
        }, false);
      });

      EventUtils.sendMouseEvent({ type: "mousedown" },
        gDebugger.document.getElementById("resume"),
        gDebugger);
    }, false);
  });

  EventUtils.sendMouseEvent({ type: "click" },
    content.document.querySelector("button"),
    content.window);
}

function resumeAndFinish() {
  gPane.panelWin.gClient.addOneTimeListener("resumed", function() {
    Services.tm.currentThread.dispatch({ run: function() {

      closeDebuggerAndFinish();

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
