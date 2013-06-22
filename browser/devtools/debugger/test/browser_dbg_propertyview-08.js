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
  gDebugger.addEventListener("Debugger:FetchedVariables", function test() {
    gDebugger.removeEventListener("Debugger:FetchedVariables", test, false);
    Services.tm.currentThread.dispatch({ run: function() {

      var frames = gDebugger.DebuggerView.StackFrames.widget._list,
          localScope = gDebugger.DebuggerView.Variables._list.querySelectorAll(".variables-view-scope")[0],
          localNodes = localScope.querySelector(".variables-view-element-details").childNodes,
          localNonEnums = localScope.querySelector(".nonenum").childNodes;

      is(gDebugger.DebuggerController.activeThread.state, "paused",
        "Should only be getting stack frames while paused.");

      is(frames.querySelectorAll(".dbg-stackframe").length, 3,
        "Should have three frames.");

      is(localNodes.length + localNonEnums.length, 12,
        "The localScope and localNonEnums should contain all the created variable elements.");

      is(localNodes[0].querySelector(".value").getAttribute("value"), "[object Window]",
        "Should have the right property value for 'this'.");
      is(localNodes[8].querySelector(".value").getAttribute("value"), "[object Arguments]",
        "Should have the right property value for 'arguments'.");
      is(localNodes[10].querySelector(".value").getAttribute("value"), "[object Object]",
        "Should have the right property value for 'c'.");


      let gVars = gDebugger.DebuggerView.Variables;

      is(gVars.getScopeForNode(
         gVars._list.querySelectorAll(".variables-view-scope")[0]).target,
         gVars._list.querySelectorAll(".variables-view-scope")[0],
        "getScopeForNode([0]) didn't return the expected scope.");
      is(gVars.getScopeForNode(
         gVars._list.querySelectorAll(".variables-view-scope")[1]).target,
         gVars._list.querySelectorAll(".variables-view-scope")[1],
        "getScopeForNode([1]) didn't return the expected scope.");
      is(gVars.getScopeForNode(
         gVars._list.querySelectorAll(".variables-view-scope")[2]).target,
         gVars._list.querySelectorAll(".variables-view-scope")[2],
        "getScopeForNode([2]) didn't return the expected scope.");

      is(gVars.getScopeForNode(gVars._list.querySelectorAll(".variables-view-scope")[0]).expanded, true,
        "The local scope should be expanded by default.");
      is(gVars.getScopeForNode(gVars._list.querySelectorAll(".variables-view-scope")[1]).expanded, false,
        "The block scope should be collapsed by default.");
      is(gVars.getScopeForNode(gVars._list.querySelectorAll(".variables-view-scope")[2]).expanded, false,
        "The global scope should be collapsed by default.");


      let thisNode = gVars.getItemForNode(localNodes[0]);
      let argumentsNode = gVars.getItemForNode(localNodes[8]);
      let cNode = gVars.getItemForNode(localNodes[10]);

      is(thisNode.expanded, false,
        "The thisNode should not be expanded at this point.");
      is(argumentsNode.expanded, false,
        "The argumentsNode should not be expanded at this point.");
      is(cNode.expanded, false,
        "The cNode should not be expanded at this point.");

      // Expand the 'this', 'arguments' and 'c' tree nodes. This causes
      // their properties to be retrieved and displayed.
      thisNode.expand();
      argumentsNode.expand();
      cNode.expand();

      is(thisNode.expanded, true,
        "The thisNode should be expanded at this point.");
      is(argumentsNode.expanded, true,
        "The argumentsNode should be expanded at this point.");
      is(cNode.expanded, true,
        "The cNode should be expanded at this point.");

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

        is(thisNode.target.querySelector(".value")
           .getAttribute("value"), "[object Window]",
          "Should have the right property value for 'this'.");

        is(thisNode.get("window").target.querySelector(".name")
           .getAttribute("value"), "window",
          "Should have the right property name for 'window'.");
        ok(thisNode.get("window").target.querySelector(".value")
           .getAttribute("value").search(/object/) != -1,
          "'window' should be an object.");

        is(thisNode.get("document").target.querySelector(".name")
           .getAttribute("value"), "document",
          "Should have the right property name for 'document'.");
        ok(thisNode.get("document").target.querySelector(".value")
           .getAttribute("value").search(/object/) != -1,
          "'document' should be an object.");


        is(argumentsNode.target.querySelector(".value")
           .getAttribute("value"), "[object Arguments]",
          "Should have the right property value for 'arguments'.");

        is(argumentsNode.target.querySelectorAll(".variables-view-property > .title > .name")[0]
           .getAttribute("value"), "0",
          "Should have the right property name for 'arguments[0]'.");
        ok(argumentsNode.target.querySelectorAll(".variables-view-property > .title > .value")[0]
           .getAttribute("value").search(/object/) != -1,
          "'arguments[0]' should be an object.");

        is(argumentsNode.target.querySelectorAll(".variables-view-property > .title > .name")[7]
           .getAttribute("value"), "__proto__",
          "Should have the right property name for '__proto__'.");
        ok(argumentsNode.target.querySelectorAll(".variables-view-property > .title > .value")[7]
           .getAttribute("value").search(/object/) != -1,
          "'__proto__' should be an object.");


        is(cNode.target.querySelector(".value")
           .getAttribute("value"), "[object Object]",
          "Should have the right property value for 'c'.");

        is(cNode.target.querySelectorAll(".variables-view-property > .title > .name")[0]
           .getAttribute("value"), "a",
          "Should have the right property name for 'c.a'.");
        is(cNode.target.querySelectorAll(".variables-view-property > .title > .value")[0]
           .getAttribute("value"), "1",
          "Should have the right value for 'c.a'.");

        is(cNode.target.querySelectorAll(".variables-view-property > .title > .name")[1]
           .getAttribute("value"), "b",
          "Should have the right property name for 'c.b'.");
        is(cNode.target.querySelectorAll(".variables-view-property > .title > .value")[1]
           .getAttribute("value"), "\"beta\"",
          "Should have the right value for 'c.b'.");

        is(cNode.target.querySelectorAll(".variables-view-property > .title > .name")[2]
           .getAttribute("value"), "c",
          "Should have the right property name for 'c.c'.");
        is(cNode.target.querySelectorAll(".variables-view-property > .title > .value")[2]
           .getAttribute("value"), "true",
          "Should have the right value for 'c.c'.");


        is(gVars.getItemForNode(
           cNode.target.querySelectorAll(".variables-view-property")[0]).target,
           cNode.target.querySelectorAll(".variables-view-property")[0],
          "getItemForNode([0]) didn't return the expected property.");

        is(gVars.getItemForNode(
           cNode.target.querySelectorAll(".variables-view-property")[1]).target,
           cNode.target.querySelectorAll(".variables-view-property")[1],
          "getItemForNode([1]) didn't return the expected property.");

        is(gVars.getItemForNode(
           cNode.target.querySelectorAll(".variables-view-property")[2]).target,
           cNode.target.querySelectorAll(".variables-view-property")[2],
          "getItemForNode([2]) didn't return the expected property.");


        is(cNode.find(
           cNode.target.querySelectorAll(".variables-view-property")[0]).target,
           cNode.target.querySelectorAll(".variables-view-property")[0],
          "find([0]) didn't return the expected property.");

        is(cNode.find(
           cNode.target.querySelectorAll(".variables-view-property")[1]).target,
           cNode.target.querySelectorAll(".variables-view-property")[1],
          "find([1]) didn't return the expected property.");

        is(cNode.find(
           cNode.target.querySelectorAll(".variables-view-property")[2]).target,
           cNode.target.querySelectorAll(".variables-view-property")[2],
          "find([2]) didn't return the expected property.");


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

    var frames = gDebugger.DebuggerView.StackFrames.widget._list;
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
