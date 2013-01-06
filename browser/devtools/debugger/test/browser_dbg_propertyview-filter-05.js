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
  function test1()
  {
    write("*one");
    ignoreExtraMatchedProperties();

    is(innerScope.querySelectorAll(".variable:not([non-match])").length, 1,
      "There should be 1 variable displayed in the inner scope");
    is(mathScope.querySelectorAll(".variable:not([non-match])").length, 0,
      "There should be 0 variables displayed in the math scope");
    is(testScope.querySelectorAll(".variable:not([non-match])").length, 0,
      "There should be 0 variables displayed in the test scope");
    is(loadScope.querySelectorAll(".variable:not([non-match])").length, 1,
      "There should be 1 variable displayed in the load scope");
    is(globalScope.querySelectorAll(".variable:not([non-match])").length, 5,
      "There should be 5 variables displayed in the global scope");

    is(innerScope.querySelectorAll(".property:not([non-match])").length, 0,
      "There should be 0 properties displayed in the inner scope");
    is(mathScope.querySelectorAll(".property:not([non-match])").length, 0,
      "There should be 0 properties displayed in the math scope");
    is(testScope.querySelectorAll(".property:not([non-match])").length, 0,
      "There should be 0 properties displayed in the test scope");
    is(loadScope.querySelectorAll(".property:not([non-match])").length, 0,
      "There should be 0 properties displayed in the load scope");
    is(globalScope.querySelectorAll(".property:not([non-match])").length, 0,
      "There should be 0 properties displayed in the global scope");

    is(innerScope.querySelectorAll(".variable:not([non-match]) > .title > .name")[0].getAttribute("value"),
      "one", "The only inner variable displayed should be 'one'");

    is(loadScope.querySelectorAll(".variable:not([non-match]) > .title > .name")[0].getAttribute("value"),
      "button", "The only load variable displayed should be 'button'");

    let oneItem = innerScopeItem.get("one");
    is(oneItem.expanded, false,
      "The one item in the inner scope should not be expanded");

    EventUtils.sendKey("RETURN");
    is(oneItem.expanded, true,
      "The one item in the inner scope should now be expanded");
  }

  function test2()
  {
    write("*two");
    ignoreExtraMatchedProperties();

    is(innerScope.querySelectorAll(".variable:not([non-match])").length, 1,
      "There should be 1 variable displayed in the inner scope");
    is(mathScope.querySelectorAll(".variable:not([non-match])").length, 0,
      "There should be 0 variables displayed in the math scope");
    is(testScope.querySelectorAll(".variable:not([non-match])").length, 0,
      "There should be 0 variables displayed in the test scope");
    is(loadScope.querySelectorAll(".variable:not([non-match])").length, 0,
      "There should be 0 variables displayed in the load scope");
    is(globalScope.querySelectorAll(".variable:not([non-match])").length, 0,
      "There should be 0 variables displayed in the global scope");

    is(innerScope.querySelectorAll(".property:not([non-match])").length, 0,
      "There should be 0 properties displayed in the inner scope");
    is(mathScope.querySelectorAll(".property:not([non-match])").length, 0,
      "There should be 0 properties displayed in the math scope");
    is(testScope.querySelectorAll(".property:not([non-match])").length, 0,
      "There should be 0 properties displayed in the test scope");
    is(loadScope.querySelectorAll(".property:not([non-match])").length, 0,
      "There should be 0 properties displayed in the load scope");
    is(globalScope.querySelectorAll(".property:not([non-match])").length, 0,
      "There should be 0 properties displayed in the global scope");

    is(innerScope.querySelectorAll(".variable:not([non-match]) > .title > .name")[0].getAttribute("value"),
      "two", "The only inner variable displayed should be 'two'");

    let twoItem = innerScopeItem.get("two");
    is(twoItem.expanded, false,
      "The two item in the inner scope should not be expanded");

    EventUtils.sendKey("RETURN");
    is(twoItem.expanded, true,
      "The two item in the inner scope should now be expanded");
  }

  function test3()
  {
    backspace(3);
    ignoreExtraMatchedProperties();

    is(gSearchBox.value, "*",
      "Searchbox value is incorrect after 3 backspaces");

    is(innerScope.querySelectorAll(".variable:not([non-match])").length, 3,
      "There should be 3 variables displayed in the inner scope");
    isnot(mathScope.querySelectorAll(".variable:not([non-match])").length, 0,
      "There should be some variables displayed in the math scope");
    isnot(testScope.querySelectorAll(".variable:not([non-match])").length, 0,
      "There should be some variables displayed in the test scope");
    isnot(loadScope.querySelectorAll(".variable:not([non-match])").length, 0,
      "There should be some variables displayed in the load scope");
    isnot(globalScope.querySelectorAll(".variable:not([non-match])").length, 0,
      "There should be some variables displayed in the global scope");

    is(innerScope.querySelectorAll(".property:not([non-match])").length, 0,
      "There should be 0 properties displayed in the inner scope");
    is(mathScope.querySelectorAll(".property:not([non-match])").length, 0,
      "There should be 0 properties displayed in the math scope");
    is(testScope.querySelectorAll(".property:not([non-match])").length, 0,
      "There should be 0 properties displayed in the test scope");
    is(loadScope.querySelectorAll(".property:not([non-match])").length, 1,
      "There should be 1 property displayed in the load scope");
    isnot(globalScope.querySelectorAll(".property:not([non-match])").length, 0,
      "There should be some properties displayed in the global scope");
  }

  function test4()
  {
    backspace(1);
    ignoreExtraMatchedProperties();

    is(gSearchBox.value, "",
      "Searchbox value is incorrect after 1 backspace");

    is(innerScope.querySelectorAll(".variable:not([non-match])").length, 3,
      "There should be 3 variables displayed in the inner scope");
    isnot(mathScope.querySelectorAll(".variable:not([non-match])").length, 0,
      "There should be some variables displayed in the math scope");
    isnot(testScope.querySelectorAll(".variable:not([non-match])").length, 0,
      "There should be some variables displayed in the test scope");
    isnot(loadScope.querySelectorAll(".variable:not([non-match])").length, 0,
      "There should be some variables displayed in the load scope");
    isnot(globalScope.querySelectorAll(".variable:not([non-match])").length, 0,
      "There should be some variables displayed in the global scope");

    is(innerScope.querySelectorAll(".property:not([non-match])").length, 0,
      "There should be 0 properties displayed in the inner scope");
    is(mathScope.querySelectorAll(".property:not([non-match])").length, 0,
      "There should be 0 properties displayed in the math scope");
    is(testScope.querySelectorAll(".property:not([non-match])").length, 0,
      "There should be 0 properties displayed in the test scope");
    is(loadScope.querySelectorAll(".property:not([non-match])").length, 1,
      "There should be 1 property displayed in the load scope");
    isnot(globalScope.querySelectorAll(".property:not([non-match])").length, 0,
      "There should be some properties displayed in the global scope");
  }

  var scopes = gDebugger.DebuggerView.Variables._list,
      innerScope = scopes.querySelectorAll(".scope")[0],
      mathScope = scopes.querySelectorAll(".scope")[1],
      testScope = scopes.querySelectorAll(".scope")[2],
      loadScope = scopes.querySelectorAll(".scope")[3],
      globalScope = scopes.querySelectorAll(".scope")[4];

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
    test1();
    executeSoon(function() {
      test2();
      executeSoon(function() {
        test3();
        executeSoon(function() {
          test4();
          executeSoon(function() {
            closeDebuggerAndFinish();
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
    // We expect 4 Debugger:FetchedVariables events, one from the global object
    // scope, two from the |with| scopes and the regular one.
    if (++count < 4) {
      info("Number of received Debugger:FetchedVariables events: " + count);
      return;
    }
    gDebugger.removeEventListener("Debugger:FetchedVariables", test, false);
    Services.tm.currentThread.dispatch({ run: function() {

      var frames = gDebugger.DebuggerView.StackFrames._container._list,
          scopes = gDebugger.DebuggerView.Variables._list,
          innerScope = scopes.querySelectorAll(".scope")[0],
          mathScope = scopes.querySelectorAll(".scope")[1],
          testScope = scopes.querySelectorAll(".scope")[2],
          loadScope = scopes.querySelectorAll(".scope")[3],
          globalScope = scopes.querySelectorAll(".scope")[4];

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

function ignoreExtraMatchedProperties()
{
  for (let [, item] of gDebugger.DebuggerView.Variables._currHierarchy) {
    let name = item.name.toLowerCase();
    let value = item._valueString || "";

    if ((value.contains("DOM")) ||
        (value.contains("XPC") && !name.contains("__proto__"))) {
      item.target.setAttribute("non-match", "");
    }
  }
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
    EventUtils.sendKey("BACK_SPACE")
  }
}

function append(text) {
  gSearchBox.focus();

  for (let i = 0; i < text.length; i++) {
    EventUtils.sendChar(text[i]);
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
