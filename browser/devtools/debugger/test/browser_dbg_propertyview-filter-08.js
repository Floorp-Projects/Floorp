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

    gDebugger.DebuggerController.StackFrames.autoScopeExpand = true;
    gDebugger.DebuggerView.Variables.delayedSearch = false;
    prepareVariables(testVariablesFiltering);
  });
}

function testVariablesFiltering()
{
  let f = {
    test1: function(aCallback)
    {
      assertExpansion(1, [true, false, false, false, false]);
      write("*arguments");
      aCallback();
    },
    test2: function(aCallback)
    {
      is(testScopeItem.get("arguments").expanded, false,
        "The arguments pseudoarray in the testScope should not be expanded");
      is(loadScopeItem.get("arguments").expanded, false,
        "The arguments pseudoarray in the testScope should not be expanded");

      assertExpansion(1, [true, true, true, true, true]);
      EventUtils.sendKey("RETURN", gDebugger);
      aCallback();
    },
    test3: function(aCallback)
    {
      is(testScopeItem.get("arguments").expanded, true,
        "The arguments pseudoarray in the testScope should now be expanded");
      is(loadScopeItem.get("arguments").expanded, true,
        "The arguments pseudoarray in the testScope should now be expanded");

      waitForFetchedProperties(2, function() {
        is(testScopeItem.get("arguments").target.querySelectorAll(".variables-view-property:not([non-match])").length, 4,
          "The arguments in the testScope should have 4 visible properties");
        is(loadScopeItem.get("arguments").target.querySelectorAll(".variables-view-property:not([non-match])").length, 4,
          "The arguments in the loadScope should have 4 visible properties");

        assertExpansion(2, [true, true, true, true, true]);
        backspace(1);
        aCallback();
      });
    },
    test4: function(aCallback)
    {
      is(testScopeItem.get("arguments").expanded, true,
        "The arguments pseudoarray in the testScope should now be expanded");
      is(loadScopeItem.get("arguments").expanded, true,
        "The arguments pseudoarray in the testScope should now be expanded");

      waitForFetchedProperties(0, function() {
        is(testScopeItem.get("arguments").target.querySelectorAll(".variables-view-property:not([non-match])").length, 4,
          "The arguments in the testScope should have 4 visible properties");
        is(loadScopeItem.get("arguments").target.querySelectorAll(".variables-view-property:not([non-match])").length, 4,
          "The arguments in the loadScope should have 4 visible properties");

        assertExpansion(3, [true, true, true, true, true]);
        backspace(8);
        aCallback();
      });
    },
    test5: function(aCallback)
    {
      is(testScopeItem.get("arguments").expanded, true,
        "The arguments pseudoarray in the testScope should now be expanded");
      is(loadScopeItem.get("arguments").expanded, true,
        "The arguments pseudoarray in the testScope should now be expanded");

      waitForFetchedProperties(0, function() {
        is(testScopeItem.get("arguments").target.querySelectorAll(".variables-view-property:not([non-match])").length, 4,
          "The arguments in the testScope should have 4 visible properties");
        is(loadScopeItem.get("arguments").target.querySelectorAll(".variables-view-property:not([non-match])").length, 4,
          "The arguments in the loadScope should have 4 visible properties");

        assertExpansion(4, [true, true, true, true, true]);
        backspace(1);
        aCallback();
      });
    },
    test6: function(aCallback)
    {
      is(testScopeItem.get("arguments").expanded, true,
        "The arguments pseudoarray in the testScope should now be expanded");
      is(loadScopeItem.get("arguments").expanded, true,
        "The arguments pseudoarray in the testScope should now be expanded");

      waitForFetchedProperties(0, function() {
        is(testScopeItem.get("arguments").target.querySelectorAll(".variables-view-property:not([non-match])").length, 4,
          "The arguments in the testScope should have 4 visible properties");
        is(loadScopeItem.get("arguments").target.querySelectorAll(".variables-view-property:not([non-match])").length, 4,
          "The arguments in the loadScope should have 4 visible properties");

        assertExpansion(5, [true, true, true, true, true]);
        write("*");
        aCallback();
      });
    },
    test7: function(aCallback)
    {
      is(testScopeItem.get("arguments").expanded, true,
        "The arguments pseudoarray in the testScope should now be expanded");
      is(loadScopeItem.get("arguments").expanded, true,
        "The arguments pseudoarray in the testScope should now be expanded");

      waitForFetchedProperties(0, function() {
        is(testScopeItem.get("arguments").target.querySelectorAll(".variables-view-property:not([non-match])").length, 4,
          "The arguments in the testScope should have 4 visible properties");
        is(loadScopeItem.get("arguments").target.querySelectorAll(".variables-view-property:not([non-match])").length, 4,
          "The arguments in the loadScope should have 4 visible properties");

        assertExpansion(5, [true, true, true, true, true]);
        append("arguments");
        aCallback();
      });
    },
    test8: function(aCallback)
    {
      is(testScopeItem.get("arguments").expanded, true,
        "The arguments pseudoarray in the testScope should now be expanded");
      is(loadScopeItem.get("arguments").expanded, true,
        "The arguments pseudoarray in the testScope should now be expanded");

      waitForFetchedProperties(0, function() {
        is(testScopeItem.get("arguments").target.querySelectorAll(".variables-view-property:not([non-match])").length, 0,
          "The arguments in the testScope should have 0 visible properties");
        is(loadScopeItem.get("arguments").target.querySelectorAll(".variables-view-property:not([non-match])").length, 0,
          "The arguments in the loadScope should have 0 visible properties");

        assertExpansion(5, [true, true, true, true, true]);
        aCallback();
      });
    },
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

  function waitForFetchedProperties(n, aCallback) {
    if (n == 0) {
      aCallback();
      return;
    }

    let count = 0;
    gDebugger.addEventListener("Debugger:FetchedProperties", function test() {
      // We expect n Debugger:FetchedProperties events.
      if (++count < n) {
        info("Number of received Debugger:FetchedVariables events: " + count);
        return;
      }
      gDebugger.removeEventListener("Debugger:FetchedProperties", test, false);
      Services.tm.currentThread.dispatch({ run: function() {
        executeSoon(aCallback);
      }}, 0);
    }, false);
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
    f.test1(function() {
      f.test2(function() {
        f.test3(function() {
          f.test4(function() {
            f.test5(function() {
              f.test6(function() {
                f.test7(function() {
                  f.test8(function() {
                    closeDebuggerAndFinish();
                  });
                });
              });
            });
          });
        });
      });
    });
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

      EventUtils.sendMouseEvent({ type: "mousedown" }, mathScope.querySelector(".arrow"), gDebugger);
      EventUtils.sendMouseEvent({ type: "mousedown" }, testScope.querySelector(".arrow"), gDebugger);
      EventUtils.sendMouseEvent({ type: "mousedown" }, loadScope.querySelector(".arrow"), gDebugger);
      EventUtils.sendMouseEvent({ type: "mousedown" }, globalScope.querySelector(".arrow"), gDebugger);

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
    EventUtils.sendKey("BACK_SPACE", gDebugger);
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
