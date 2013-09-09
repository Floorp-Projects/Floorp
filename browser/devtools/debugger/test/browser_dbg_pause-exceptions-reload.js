/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that pausing on exceptions works after reload.
 */

const TAB_URL = EXAMPLE_URL + "test-pause-exceptions-reload.html";

var gPane = null;
var gTab = null;
var gDebugger = null;
var gPrevPref = null;

function test()
{
  gPrevPref = Services.prefs.getBoolPref(
    "devtools.debugger.pause-on-exceptions");
  Services.prefs.setBoolPref(
    "devtools.debugger.pause-on-exceptions", true);

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
  // Pause on exceptions should be already enabled.
  is(gDebugger.Prefs.pauseOnExceptions, true,
    "The pause-on-exceptions pref should be true from startup.");
  is(gDebugger.DebuggerView.Options._pauseOnExceptionsItem.getAttribute("checked"), "true",
    "Pause on exceptions should be enabled from startup. ")

  // Disable ignore caught exceptions
  gDebugger.DebuggerView.Options._ignoreCaughtExceptionsItem.setAttribute("checked", "false");
  gDebugger.DebuggerView.Options._toggleIgnoreCaughtExceptions();

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

      count = 0;
      gPane.panelWin.gClient.addOneTimeListener("paused", function() {
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

            var frames = gDebugger.DebuggerView.StackFrames.widget._list,
                scopes = gDebugger.DebuggerView.Variables._list,
                innerScope = scopes.firstChild,
                innerNodes = innerScope.querySelector(".variables-view-element-details").childNodes;

            is(gDebugger.DebuggerController.activeThread.state, "paused",
              "Should be paused again.");

            is(frames.querySelectorAll(".dbg-stackframe").length, 2,
              "Should have two frames.");

            is(scopes.children.length, 2, "Should have 2 variable scopes.");

            is(innerNodes[0].querySelector(".name").getAttribute("value"), "<exception>",
              "Should have the right property name for the exception.");

            is(innerNodes[0].querySelector(".value").getAttribute("value"), "Error",
              "Should have the right property value for the exception.");

            resumeAndFinish();

          }}, 0);
        }, false);
      });

      content.window.location.reload();
    }, false);
  });

  content.window.location.reload();
}

function resumeAndFinish() {
  // Disable pause on exceptions.
  gDebugger.DebuggerView.Options._pauseOnExceptionsItem.setAttribute("checked", "false");
  gDebugger.DebuggerView.Options._togglePauseOnExceptions();

  // Enable ignore caught exceptions
  gDebugger.DebuggerView.Options._ignoreCaughtExceptionsItem.setAttribute("checked", "true");
  gDebugger.DebuggerView.Options._toggleIgnoreCaughtExceptions();

  is(gDebugger.Prefs.pauseOnExceptions, false,
    "The pause-on-exceptions pref should have been set to false.");

  gPane.panelWin.gClient.addOneTimeListener("resumed", function() {
    Services.tm.currentThread.dispatch({ run: closeDebuggerAndFinish }, 0);
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
