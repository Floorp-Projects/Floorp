/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the property view correctly filters nodes.
 */

const TAB_URL = EXAMPLE_URL + "browser_dbg_with-frame.html";

var gPane = null;
var gTab = null;
var gDebugger = null;
var gDebuggee = null;
var gSearchBox = null;

requestLongerTimeout(2);

function test()
{
  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gPane = aPane;
    gDebugger = gPane.panelWin;
    gDebuggee = aDebuggee;

    gDebugger.DebuggerController.StackFrames.autoScopeExpand = false;
    gDebugger.DebuggerView.Variables.delayedSearch = false;
    prepareVariables(testVariablesFiltering);
  });
}

function testVariablesFiltering()
{
  let f = {
    test1: function()
    {
      assertExpansion(1, [true, false, false, false, false]);
      clear();
    },
    test2: function()
    {
      assertExpansion(2, [true, false, false, false, false]);
      EventUtils.sendKey("RETURN", gDebugger);
    },
    test3: function()
    {
      assertExpansion(3, [true, false, false, false, false]);
      gDebugger.editor.focus();
    },
    test4: function()
    {
      assertExpansion(4, [true, false, false, false, false]);
      write("*");
    },
    test5: function() {
      assertExpansion(5, [true, true, true, true, true]);
      EventUtils.sendKey("RETURN", gDebugger);
    },
    test6: function() {
      assertExpansion(6, [true, true, true, true, true]);
      gDebugger.editor.focus();
    },
    test7: function() {
      assertExpansion(7, [true, true, true, true, true]);
      backspace(1);
    },
    test8: function() {
      assertExpansion(8, [true, true, true, true, true]);
      EventUtils.sendKey("RETURN", gDebugger);
    },
    test9: function() {
      assertExpansion(9, [true, true, true, true, true]);
      gDebugger.editor.focus();
    },
    test10: function() {
      assertExpansion(10, [true, true, true, true, true]);
      innerScopeItem.collapse();
      mathScopeItem.collapse();
      testScopeItem.collapse();
      loadScopeItem.collapse();
      globalScopeItem.collapse();
    },
    test11: function() {
      assertExpansion(11, [false, false, false, false, false]);
      clear();
    },
    test12: function() {
      assertExpansion(12, [false, false, false, false, false]);
      EventUtils.sendKey("RETURN", gDebugger);
    },
    test13: function() {
      assertExpansion(13, [false, false, false, false, false]);
      gDebugger.editor.focus();
    },
    test14: function() {
      assertExpansion(14, [false, false, false, false, false]);
      write("*");
    },
    test15: function() {
      assertExpansion(15, [true, true, true, true, true]);
      EventUtils.sendKey("RETURN", gDebugger);
    },
    test16: function() {
      assertExpansion(16, [true, true, true, true, true]);
      gDebugger.editor.focus();
    },
    test17: function() {
      assertExpansion(17, [true, true, true, true, true]);
      backspace(1);
    },
    test18: function() {
      assertExpansion(18, [true, true, true, true, true]);
      EventUtils.sendKey("RETURN", gDebugger);
    },
    test19: function() {
      assertExpansion(19, [true, true, true, true, true]);
      gDebugger.editor.focus();
    },
    test20: function() {
      assertExpansion(20, [true, true, true, true, true]);
    }
  };

  function assertExpansion(n, array) {
    is(innerScopeItem.expanded, array[0],
      "The innerScope should " + (array[0] ? "" : "not ") +
       "be expanded at this point (" + n + ")");

    is(mathScopeItem.expanded, array[1],
      "The mathScope should " + (array[1] ? "" : "not ") +
       "be expanded at this point (" + n + ")");

    is(testScopeItem.expanded, array[2],
      "The testScope should " + (array[2] ? "" : "not ") +
       "be expanded at this point (" + n + ")");

    is(loadScopeItem.expanded, array[3],
      "The loadScope should " + (array[3] ? "" : "not ") +
       "be expanded at this point (" + n + ")");

    is(globalScopeItem.expanded, array[4],
      "The globalScope should " + (array[4] ? "" : "not ") +
       "be expanded at this point (" + n + ")");
  }

  var scopes = gDebugger.DebuggerView.Variables._list,
      innerScope = scopes.querySelectorAll(".variables-view-scope")[0],
      mathScope = scopes.querySelectorAll(".variables-view-scope")[1],
      testScope = scopes.querySelectorAll(".variables-view-scope")[2],
      loadScope = scopes.querySelectorAll(".variables-view-scope")[3],
      globalScope = scopes.querySelectorAll(".variables-view-scope")[4];

  let innerScopeItem = gDebugger.DebuggerView.Variables._currHierarchy.get(
    innerScope.querySelector(".name").getAttribute("value"));
  let mathScopeItem = gDebugger.DebuggerView.Variables._currHierarchy.get(
    mathScope.querySelector(".name").getAttribute("value"));
  let testScopeItem = gDebugger.DebuggerView.Variables._currHierarchy.get(
    testScope.querySelector(".name").getAttribute("value"));
  let loadScopeItem = gDebugger.DebuggerView.Variables._currHierarchy.get(
    loadScope.querySelector(".name").getAttribute("value"));
  let globalScopeItem = gDebugger.DebuggerView.Variables._currHierarchy.get(
    globalScope.querySelector(".name").getAttribute("value"));

  gSearchBox = gDebugger.DebuggerView.Filtering._searchbox;

  executeSoon(function() {
    for (let i = 1; i <= Object.keys(f).length; i++) {
      f["test" + i]();
    }
    closeDebuggerAndFinish();
  });
}

function prepareVariables(aCallback)
{
  let count = 0;
  gDebugger.addEventListener("Debugger:FetchedVariables", function test() {
    // We expect 2 Debugger:FetchedVariables events, one from the inner object
    // scope and the regular one.
    if (++count < 2) {
      info("Number of received Debugger:FetchedVariables events: " + count);
      return;
    }
    gDebugger.removeEventListener("Debugger:FetchedVariables", test, false);
    Services.tm.currentThread.dispatch({ run: function() {

      var frames = gDebugger.DebuggerView.StackFrames.widget._list,
          scopes = gDebugger.DebuggerView.Variables._list,
          innerScope = scopes.querySelectorAll(".variables-view-scope")[0],
          mathScope = scopes.querySelectorAll(".variables-view-scope")[1],
          testScope = scopes.querySelectorAll(".variables-view-scope")[2],
          loadScope = scopes.querySelectorAll(".variables-view-scope")[3],
          globalScope = scopes.querySelectorAll(".variables-view-scope")[4];

      let innerScopeItem = gDebugger.DebuggerView.Variables._currHierarchy.get(
        innerScope.querySelector(".name").getAttribute("value"));
      let mathScopeItem = gDebugger.DebuggerView.Variables._currHierarchy.get(
        mathScope.querySelector(".name").getAttribute("value"));
      let testScopeItem = gDebugger.DebuggerView.Variables._currHierarchy.get(
        testScope.querySelector(".name").getAttribute("value"));
      let loadScopeItem = gDebugger.DebuggerView.Variables._currHierarchy.get(
        loadScope.querySelector(".name").getAttribute("value"));
      let globalScopeItem = gDebugger.DebuggerView.Variables._currHierarchy.get(
        globalScope.querySelector(".name").getAttribute("value"));

      executeSoon(function() {
        aCallback();
      });
    }}, 0);
  }, false);

  EventUtils.sendMouseEvent({ type: "click" },
    gDebuggee.document.querySelector("button"),
    gDebuggee.window);
}

function clear() {
  gSearchBox.focus();
  gSearchBox.value = "";
}

function write(text) {
  clear();
  append(text);
}

function backspace(times) {
  for (let i = 0; i < times; i++) {
    EventUtils.sendKey("BACK_SPACE", gDebugger)
  }
}

function append(text) {
  gSearchBox.focus();

  for (let i = 0; i < text.length; i++) {
    EventUtils.sendChar(text[i], gDebugger);
  }
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebugger = null;
  gDebuggee = null;
  gSearchBox = null;
});
