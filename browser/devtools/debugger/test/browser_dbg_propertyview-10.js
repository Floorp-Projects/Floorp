/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the property view is correctly populated in |with| frames.
 */

const TAB_URL = EXAMPLE_URL + "browser_dbg_with-frame.html";

var gPane = null;
var gTab = null;
var gDebugger = null;

requestLongerTimeout(2);

function test()
{
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
  gDebugger.addEventListener("Debugger:FetchedVariables", function test() {
    // We expect 4 Debugger:FetchedVariables events, one from the global object
    // scope, two from the |with| scopes and the regular one.
    if (++count < 4) {
      info("Number of received Debugger:FetchedVariables events: " + count);
      return;
    }
    gDebugger.removeEventListener("Debugger:FetchedVariables", test, false);
    Services.tm.currentThread.dispatch({ run: function() {

      var frames = gDebugger.DebuggerView.StackFrames.widget._list,
          scopes = gDebugger.DebuggerView.Variables._list,
          innerScope = scopes.querySelectorAll(".variables-view-scope")[0],
          globalScope = scopes.querySelectorAll(".variables-view-scope")[4],
          innerNodes = innerScope.querySelector(".variables-view-element-details").childNodes,
          globalNodes = globalScope.querySelector(".variables-view-element-details").childNodes;

      is(gDebugger.DebuggerController.activeThread.state, "paused",
        "Should only be getting stack frames while paused.");

      is(frames.querySelectorAll(".dbg-stackframe").length, 2,
        "Should have three frames.");

      is(scopes.childNodes.length, 5, "Should have 5 variable scopes.");

      is(innerNodes[1].querySelector(".name").getAttribute("value"), "one",
        "Should have the right property name for |one|.");

      is(innerNodes[1].querySelector(".value").getAttribute("value"), "1",
        "Should have the right property value for |one|.");

      let globalScopeObject = gDebugger.DebuggerView.Variables.getScopeForNode(globalScope);
      let documentNode = globalScopeObject.get("document");

      is(documentNode.target.querySelector(".name").getAttribute("value"), "document",
        "Should have the right property name for |document|.");

      is(documentNode.target.querySelector(".value").getAttribute("value"), "HTMLDocument",
        "Should have the right property value for |document|.");

      let len = globalNodes.length - 1;
      is(globalNodes[len].querySelector(".name").getAttribute("value"), "window",
        "Should have the right property name for |window|.");

      is(globalNodes[len].querySelector(".value").getAttribute("value"), "Window",
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
      var frames = gDebugger.DebuggerView.StackFrames.widget._list;

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
