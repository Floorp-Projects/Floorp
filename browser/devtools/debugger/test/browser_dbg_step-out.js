/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that stepping out of a function displays the right return value.
 */

const TAB_URL = EXAMPLE_URL + "test-step-out.html";

var gPane = null;
var gTab = null;
var gDebugger = null;

function test()
{
  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gPane = aPane;
    gDebugger = gPane.panelWin;

    testNormalReturn();
  });
}

function testNormalReturn()
{
  gPane.panelWin.gClient.addOneTimeListener("paused", function() {
    gDebugger.addEventListener("Debugger:SourceShown", function dbgstmt(aEvent) {
      gDebugger.removeEventListener(aEvent.type, dbgstmt);
      is(gDebugger.DebuggerController.activeThread.state, "paused",
        "Should be paused now.");

      let count = 0;
      gPane.panelWin.gClient.addOneTimeListener("paused", function() {
        is(gDebugger.DebuggerController.activeThread.state, "paused",
          "Should be paused again.");

        gDebugger.addEventListener("Debugger:FetchedVariables", function stepout() {
          ok(true, "Debugger:FetchedVariables event received.");
          gDebugger.removeEventListener("Debugger:FetchedVariables", stepout, false);

          Services.tm.currentThread.dispatch({ run: function() {

            var scopes = gDebugger.DebuggerView.Variables._list,
                innerScope = scopes.firstChild,
                innerNodes = innerScope.querySelector(".variables-view-element-details").childNodes;

            is(innerNodes[0].querySelector(".name").getAttribute("value"), "<return>",
              "Should have the right property name for the return value.");

            is(innerNodes[0].querySelector(".value").getAttribute("value"), 10,
              "Should have the right property value for the return value.");

            testReturnWithException();
          }}, 0);
        }, false);
      });

      EventUtils.sendMouseEvent({ type: "mousedown" },
        gDebugger.document.getElementById("step-out"),
        gDebugger);
    });
  });

  EventUtils.sendMouseEvent({ type: "click" },
    content.document.getElementById("return"),
    content.window);
}

function testReturnWithException()
{
  gDebugger.DebuggerController.activeThread.resume(function() {
    gPane.panelWin.gClient.addOneTimeListener("paused", function() {
      gDebugger.addEventListener("Debugger:FetchedVariables", function dbgstmt(aEvent) {
        gDebugger.removeEventListener(aEvent.type, dbgstmt, false);
        is(gDebugger.DebuggerController.activeThread.state, "paused",
          "Should be paused now.");

        let count = 0;
        gPane.panelWin.gClient.addOneTimeListener("paused", function() {
          is(gDebugger.DebuggerController.activeThread.state, "paused",
            "Should be paused again.");

          gDebugger.addEventListener("Debugger:FetchedVariables", function stepout() {
            ok(true, "Debugger:FetchedVariables event received.");
            gDebugger.removeEventListener("Debugger:FetchedVariables", stepout, false);

            Services.tm.currentThread.dispatch({ run: function() {

              var scopes = gDebugger.DebuggerView.Variables._list,
                  innerScope = scopes.firstChild,
                  innerNodes = innerScope.querySelector(".variables-view-element-details").childNodes;

              is(innerNodes[0].querySelector(".name").getAttribute("value"), "<exception>",
                "Should have the right property name for the exception value.");

              is(innerNodes[0].querySelector(".value").getAttribute("value"), '"boom"',
                "Should have the right property value for the exception value.");

              resumeAndFinish();

            }}, 0);
          }, false);
        });

        EventUtils.sendMouseEvent({ type: "mousedown" },
          gDebugger.document.getElementById("step-out"),
          gDebugger);
      }, false);
    });

    EventUtils.sendMouseEvent({ type: "click" },
      content.document.getElementById("throw"),
      content.window);
  });
}

function resumeAndFinish() {
  gPane.panelWin.gClient.addOneTimeListener("resumed", function() {
    Services.tm.currentThread.dispatch({ run: closeDebuggerAndFinish }, 0);
  });

  gDebugger.DebuggerController.activeThread.resume();
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebugger = null;
});
