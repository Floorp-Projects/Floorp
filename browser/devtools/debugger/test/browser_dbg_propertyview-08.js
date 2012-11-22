/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the property view displays the properties of objects.
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
  dump("Started testFrameParameters!\n");

  gDebugger.addEventListener("Debugger:FetchedVariables", function test() {
    dump("Entered Debugger:FetchedVariables!\n");

    gDebugger.removeEventListener("Debugger:FetchedVariables", test, false);
    Services.tm.currentThread.dispatch({ run: function() {

      dump("After currentThread.dispatch!\n");

      var frames = gDebugger.DebuggerView.StackFrames._container._list,
          localScope = gDebugger.DebuggerView.Variables._list.querySelectorAll(".scope")[0],
          localNodes = localScope.querySelector(".details").childNodes,
          localNonEnums = localScope.querySelector(".nonenum").childNodes;

      dump("Got our variables:\n");
      dump("frames     - " + frames.constructor + "\n");
      dump("localScope - " + localScope.constructor + "\n");
      dump("localNodes - " + localNodes.constructor + "\n");
      dump("localNonEnums - " + localNonEnums.constructor + "\n");

      is(gDebugger.DebuggerController.activeThread.state, "paused",
        "Should only be getting stack frames while paused.");

      is(frames.querySelectorAll(".dbg-stackframe").length, 3,
        "Should have three frames.");

      is(localNodes.length + localNonEnums.length, 11,
        "The localScope and localNonEnums should contain all the created variable elements.");

      is(localNodes[0].querySelector(".value").getAttribute("value"), "[object Proxy]",
        "Should have the right property value for 'this'.");

      let thisNode, argumentsNode, cNode;
      for (let [id, scope] in gDebugger.DebuggerView.Variables) {
        if (scope.target === localScope) {
          for (let [name, variable] in scope) {
            if (variable.target === localNodes[0]) {
              thisNode = variable;
            }
            if (variable.target === localNodes[8]) {
              argumentsNode = variable;
            }
            if (variable.target === localNodes[10]) {
              cNode = variable;
            }
          }
        }
      }

      // Expand the 'this', 'arguments' and 'c' tree nodes. This causes
      // their properties to be retrieved and displayed.
      thisNode.expand();
      argumentsNode.expand();
      cNode.expand();

      // Poll every few milliseconds until the properties are retrieved.
      // It's important to set the timer in the chrome window, because the
      // content window timers are disabled while the debuggee is paused.
      let count = 0;
      let intervalID = window.setInterval(function(){
        info("count: " + count + " ");
        if (++count > 50) {
          ok(false, "Timed out while polling for the properties.");
          window.clearInterval(intervalID);
          return resumeAndFinish();
        }
        if (!thisNode._retrieved ||
            !argumentsNode._retrieved ||
            !cNode._retrieved) {
          return;
        }
        window.clearInterval(intervalID);
        is(thisNode.target.querySelector(".property > .title > .name")
                        .getAttribute("value"), "InstallTrigger",
          "Should have the right property name for InstallTrigger.");
        ok(thisNode.target.querySelector(".property > .title > .value")
                        .getAttribute("value").search(/object/) == -1,
          "InstallTrigger should not be an object.");

        is(argumentsNode.target.querySelector(".value")
                        .getAttribute("value"), "[object Arguments]",
         "Should have the right property value for 'arguments'.");
        ok(argumentsNode.target.querySelector(".property > .title > .value")
                        .getAttribute("value").search(/object/) != -1,
          "Arguments should be an object.");

        is(argumentsNode.target.querySelectorAll(".property > .title > .name")[7]
                        .getAttribute("value"), "__proto__",
         "Should have the right property name for '__proto__'.");
        ok(argumentsNode.target.querySelectorAll(".property > .title > .value")[7]
                        .getAttribute("value").search(/object/) != -1,
          "__proto__ should be an object.");

        is(cNode.target.querySelector(".value")
                         .getAttribute("value"), "[object Object]",
          "Should have the right property value for 'c'.");

        is(cNode.target.querySelectorAll(".property > .title > .name")[0]
                         .getAttribute("value"), "a",
          "Should have the right property name for 'c.a'.");
        is(cNode.target.querySelectorAll(".property > .title > .value")[0]
                         .getAttribute("value"), "1",
          "Should have the right value for 'c.a'.");

        is(cNode.target.querySelectorAll(".property > .title > .name")[1]
                         .getAttribute("value"), "b",
          "Should have the right property name for 'c.b'.");
        is(cNode.target.querySelectorAll(".property > .title > .value")[1]
                         .getAttribute("value"), "\"beta\"",
          "Should have the right value for 'c.b'.");

        is(cNode.target.querySelectorAll(".property > .title > .name")[2]
                         .getAttribute("value"), "c",
          "Should have the right property name for 'c.c'.");
        is(cNode.target.querySelectorAll(".property > .title > .value")[2]
                         .getAttribute("value"), "true",
          "Should have the right value for 'c.c'.");

        resumeAndFinish();
      }, 100);
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
