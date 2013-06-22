/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Make sure that the editing or removing watch expressions works properly.
 */

const TAB_URL = EXAMPLE_URL + "browser_dbg_watch-expressions.html";

var gPane = null;
var gTab = null;
var gDebuggee = null;
var gDebugger = null;
var gWatch = null;
var gVars = null;

requestLongerTimeout(3);

function test() {
  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.panelWin;
    gWatch = gDebugger.DebuggerView.WatchExpressions;
    gVars = gDebugger.DebuggerView.Variables;

    gDebugger.DebuggerController.StackFrames.autoScopeExpand = true;
    gDebugger.DebuggerView.Variables.nonEnumVisible = false;
    testFrameEval();
  });
}

function testFrameEval() {
  gDebugger.addEventListener("Debugger:FetchedWatchExpressions", function test() {
    gDebugger.removeEventListener("Debugger:FetchedWatchExpressions", test, false);
    Services.tm.currentThread.dispatch({ run: function() {

      is(gDebugger.DebuggerController.activeThread.state, "paused",
        "Should only be getting stack frames while paused.");

      var localScope = gDebugger.DebuggerView.Variables._list.querySelectorAll(".variables-view-scope")[1],
          localNodes = localScope.querySelector(".variables-view-element-details").childNodes,
          aArg = localNodes[1],
          varT = localNodes[3];

      is(aArg.querySelector(".name").getAttribute("value"), "aArg",
        "Should have the right name for 'aArg'.");
      is(varT.querySelector(".name").getAttribute("value"), "t",
        "Should have the right name for 't'.");

      is(aArg.querySelector(".value").getAttribute("value"), "undefined",
        "Should have the right initial value for 'aArg'.");
      is(varT.querySelector(".value").getAttribute("value"), "\"Browser Debugger Watch Expressions Test\"",
        "Should have the right initial value for 't'.");

      is(gWatch.widget._parent.querySelectorAll(".dbg-expression[hidden=true]").length, 5,
        "There should be 5 hidden nodes in the watch expressions container");
      is(gWatch.widget._parent.querySelectorAll(".dbg-expression:not([hidden=true])").length, 0,
        "There should be 0 visible nodes in the watch expressions container");

      let label = gDebugger.L10N.getStr("watchExpressionsScopeLabel");
      let scope = gVars._currHierarchy.get(label);

      ok(scope, "There should be a wach expressions scope in the variables view");
      is(scope._store.size, 5, "There should be 5 evaluations availalble");

      is(scope.get("this")._isContentVisible, true,
        "Should have the right visibility state for 'this'.");
      is(scope.get("this").target.querySelectorAll(".variables-view-delete").length, 1,
        "Should have the one close button visible for 'this'.");
      is(scope.get("this").name, "this",
        "Should have the right name for 'this'.");
      is(scope.get("this").value.type, "object",
        "Should have the right value type for 'this'.");
      is(scope.get("this").value.class, "Window",
        "Should have the right value type for 'this'.");

      is(scope.get("ermahgerd")._isContentVisible, true,
        "Should have the right visibility state for 'ermahgerd'.");
      is(scope.get("ermahgerd").target.querySelectorAll(".variables-view-delete").length, 1,
        "Should have the one close button visible for 'ermahgerd'.");
      is(scope.get("ermahgerd").name, "ermahgerd",
        "Should have the right name for 'ermahgerd'.");
      is(scope.get("ermahgerd").value.type, "object",
        "Should have the right value type for 'ermahgerd'.");
      is(scope.get("ermahgerd").value.class, "Function",
        "Should have the right value type for 'ermahgerd'.");

      is(scope.get("aArg")._isContentVisible, true,
        "Should have the right visibility state for 'aArg'.");
      is(scope.get("aArg").target.querySelectorAll(".variables-view-delete").length, 1,
        "Should have the one close button visible for 'aArg'.");
      is(scope.get("aArg").name, "aArg",
        "Should have the right name for 'aArg'.");
      is(scope.get("aArg").value.type, "undefined",
        "Should have the right value for 'aArg'.");
      is(scope.get("aArg").value.class, undefined,
        "Should have the right value for 'aArg'.");

      is(scope.get("document.title")._isContentVisible, true,
        "Should have the right visibility state for 'document.title'.");
      is(scope.get("document.title").target.querySelectorAll(".variables-view-delete").length, 1,
        "Should have the one close button visible for 'document.title'.");
      is(scope.get("document.title").name, "document.title",
        "Should have the right name for 'document.title'.");
      is(scope.get("document.title").value, "42",
        "Should have the right value for 'document.title'.");
      is(typeof scope.get("document.title").value, "string",
        "Should have the right value type for 'document.title'.");

      is(scope.get("document.title = 42")._isContentVisible, true,
        "Should have the right visibility state for 'document.title = 42'.");
      is(scope.get("document.title = 42").target.querySelectorAll(".variables-view-delete").length, 1,
        "Should have the one close button visible for 'document.title = 42'.");
      is(scope.get("document.title = 42").name, "document.title = 42",
        "Should have the right name for 'document.title = 42'.");
      is(scope.get("document.title = 42").value, 42,
        "Should have the right value for 'document.title = 42'.");
      is(typeof scope.get("document.title = 42").value, "number",
        "Should have the right value type for 'document.title = 42'.");

      testModification(scope.get("document.title = 42").target, test1, function(scope) {
        testModification(scope.get("aArg").target, test2, function(scope) {
          testModification(scope.get("aArg = 44").target, test3, function(scope) {
            testModification(scope.get("document.title = 43").target, test4, function(scope) {
              testModification(scope.get("document.title").target, test5, function(scope) {
                testExprDeletion(scope.get("this").target, test6, function(scope) {
                  testExprDeletion(null, test7, function(scope) {
                    resumeAndFinish();
                  }, 44, 0, true, true);
                }, 44);
              }, " \t\r\n", "\"43\"", 44, 1, true);
            }, " \t\r\ndocument.title \t\r\n", "\"43\"", 44);
          }, " \t\r\ndocument.title \t\r\n", "\"43\"", 44);
        }, "aArg = 44", 44, 44);
      }, "document.title = 43", 43, "undefined");
    }}, 0);
  }, false);

  addWatchExpression("this");
  addWatchExpression("ermahgerd");
  addWatchExpression("aArg");
  addWatchExpression("document.title");
  addCmdWatchExpression("document.title = 42");

  executeSoon(function() {
    gDebuggee.ermahgerd(); // ermahgerd!!
  });
}

function testModification(aVar, aTest, aCallback, aNewValue, aNewResult, aArgResult,
                          aLocalScopeIndex = 1, aDeletionFlag = null)
{
  function makeChangesAndExitInputMode() {
    EventUtils.sendString(aNewValue, gDebugger);
    EventUtils.sendKey("RETURN", gDebugger);
  }

  EventUtils.sendMouseEvent({ type: "dblclick" },
    aVar.querySelector(".name"),
    gDebugger);

  executeSoon(function() {
    ok(aVar.querySelector(".element-name-input"),
      "There should be an input element created.");

    let testContinued = false;
    let fetchedVariables = false;
    let fetchedExpressions = false;

    let countV = 0;
    gDebugger.addEventListener("Debugger:FetchedVariables", function testV() {
      // We expect 2 Debugger:FetchedVariables events, one from the global
      // object scope and the regular one.
      if (++countV < 2) {
        info("Number of received Debugger:FetchedVariables events: " + countV);
        return;
      }
      gDebugger.removeEventListener("Debugger:FetchedVariables", testV, false);
      fetchedVariables = true;
      executeSoon(continueTest);
    }, false);

    let countE = 0;
    gDebugger.addEventListener("Debugger:FetchedWatchExpressions", function testE() {
      // We expect only one Debugger:FetchedWatchExpressions event, since all
      // expressions are evaluated at the same time.
      if (++countE < 1) {
        info("Number of received Debugger:FetchedWatchExpressions events: " + countE);
        return;
      }
      gDebugger.removeEventListener("Debugger:FetchedWatchExpressions", testE, false);
      fetchedExpressions = true;
      executeSoon(continueTest);
    }, false);

    function continueTest() {
      if (testContinued || !fetchedVariables || !fetchedExpressions) {
        return;
      }
      testContinued = true;

      // Get the variable reference anew, since the old ones were discarded when
      // we resumed.
      var localScope = gDebugger.DebuggerView.Variables._list.querySelectorAll(".variables-view-scope")[aLocalScopeIndex],
          localNodes = localScope.querySelector(".variables-view-element-details").childNodes,
          aArg = localNodes[1];

      is(aArg.querySelector(".value").getAttribute("value"), aArgResult,
        "Should have the right value for 'aArg'.");

      let label = gDebugger.L10N.getStr("watchExpressionsScopeLabel");
      let scope = gVars._currHierarchy.get(label);
      info("Found the watch expressions scope: " + scope);

      let aExp = scope.get(aVar.querySelector(".name").getAttribute("value"));
      info("Found the watch expression variable: " + aExp);

      if (aDeletionFlag) {
        ok(fetchedVariables, "The variables should have been fetched.");
        ok(fetchedExpressions, "The variables should have been fetched.");
        is(aExp, undefined, "The watch expression should not have been found.");
        performCallback(scope);
        return;
      }

      is(aExp.target.querySelector(".name").getAttribute("value"), aNewValue.trim(),
        "Should have the right name for '" + aNewValue + "'.");
      is(aExp.target.querySelector(".value").getAttribute("value"), aNewResult,
        "Should have the right value for '" + aNewValue + "'.");

      performCallback(scope);
    }

    makeChangesAndExitInputMode();
  });

  function performCallback(scope) {
    executeSoon(function() {
      aTest(scope);
      aCallback(scope);
    });
  }
}

function testExprDeletion(aVar, aTest, aCallback, aArgResult,
                          aLocalScopeIndex = 1, aFinalFlag = null, aRemoveAllFlag = null)
{
  let testContinued = false;
  let fetchedVariables = false;
  let fetchedExpressions = false;

  let countV = 0;
  gDebugger.addEventListener("Debugger:FetchedVariables", function testV() {
    // We expect 2 Debugger:FetchedVariables events, one from the global
    // object scope and the regular one.
    if (++countV < 2) {
      info("Number of received Debugger:FetchedVariables events: " + countV);
      return;
    }
    gDebugger.removeEventListener("Debugger:FetchedVariables", testV, false);
    fetchedVariables = true;
    executeSoon(continueTest);
  }, false);

  let countE = 0;
  gDebugger.addEventListener("Debugger:FetchedWatchExpressions", function testE() {
    // We expect only one Debugger:FetchedWatchExpressions event, since all
    // expressions are evaluated at the same time.
    if (++countE < 1) {
      info("Number of received Debugger:FetchedWatchExpressions events: " + countE);
      return;
    }
    gDebugger.removeEventListener("Debugger:FetchedWatchExpressions", testE, false);
    fetchedExpressions = true;
    executeSoon(continueTest);
  }, false);

  function continueTest() {
    if ((testContinued || !fetchedVariables || !fetchedExpressions) && !aFinalFlag) {
      return;
    }
    testContinued = true;

    // Get the variable reference anew, since the old ones were discarded when
    // we resumed.
    var localScope = gDebugger.DebuggerView.Variables._list.querySelectorAll(".variables-view-scope")[aLocalScopeIndex],
        localNodes = localScope.querySelector(".variables-view-element-details").childNodes,
        aArg = localNodes[1];

    is(aArg.querySelector(".value").getAttribute("value"), aArgResult,
      "Should have the right value for 'aArg'.");

    let label = gDebugger.L10N.getStr("watchExpressionsScopeLabel");
    let scope = gVars._currHierarchy.get(label);
    info("Found the watch expressions scope: " + scope);

    if (aFinalFlag) {
      ok(fetchedVariables, "The variables should have been fetched.");
      ok(!fetchedExpressions, "The variables should never have been fetched.");
      is(scope, undefined, "The watch expressions scope should not have been found.");
      performCallback(scope);
      return;
    }

    let aExp = scope.get(aVar.querySelector(".name").getAttribute("value"));
    info("Found the watch expression variable: " + aExp);

    is(aExp, undefined, "Should not have found the watch expression after deletion.");
    performCallback(scope);
  }

  function performCallback(scope) {
    executeSoon(function() {
      aTest(scope);
      aCallback(scope);
    });
  }

  if (aRemoveAllFlag) {
    gWatch._onCmdRemoveAllExpressions();
    return;
  }

  EventUtils.sendMouseEvent({ type: "click" },
    aVar.querySelector(".variables-view-delete"),
    gDebugger);
}

function test1(scope) {
  is(gWatch.widget._parent.querySelectorAll(".dbg-expression[hidden=true]").length, 5,
    "There should be 5 hidden nodes in the watch expressions container");
  is(gWatch.widget._parent.querySelectorAll(".dbg-expression:not([hidden=true])").length, 0,
    "There should be 0 visible nodes in the watch expressions container");

  ok(scope, "There should be a wach expressions scope in the variables view");
  is(scope._store.size, 5, "There should be 5 evaluations availalble");

  is(gWatch.getItemAtIndex(0).attachment.inputNode.value, "document.title = 43",
    "The first textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(0).attachment.currentExpression, "document.title = 43",
    "The first textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(1).attachment.inputNode.value, "document.title",
    "The second textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(1).attachment.currentExpression, "document.title",
    "The second textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(2).attachment.inputNode.value, "aArg",
    "The third textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(2).attachment.currentExpression, "aArg",
    "The third textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(3).attachment.inputNode.value, "ermahgerd",
    "The fourth textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(3).attachment.currentExpression, "ermahgerd",
    "The fourth textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(4).attachment.inputNode.value, "this",
    "The fifth textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(4).attachment.currentExpression, "this",
    "The fifth textbox input value is not the correct one");
}

function test2(scope) {
  is(gWatch.widget._parent.querySelectorAll(".dbg-expression[hidden=true]").length, 5,
    "There should be 5 hidden nodes in the watch expressions container");
  is(gWatch.widget._parent.querySelectorAll(".dbg-expression:not([hidden=true])").length, 0,
    "There should be 0 visible nodes in the watch expressions container");

  ok(scope, "There should be a wach expressions scope in the variables view");
  is(scope._store.size, 5, "There should be 5 evaluations availalble");

  is(gWatch.getItemAtIndex(0).attachment.inputNode.value, "document.title = 43",
    "The first textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(0).attachment.currentExpression, "document.title = 43",
    "The first textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(1).attachment.inputNode.value, "document.title",
    "The second textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(1).attachment.currentExpression, "document.title",
    "The second textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(2).attachment.inputNode.value, "aArg = 44",
    "The third textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(2).attachment.currentExpression, "aArg = 44",
    "The third textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(3).attachment.inputNode.value, "ermahgerd",
    "The fourth textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(3).attachment.currentExpression, "ermahgerd",
    "The fourth textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(4).attachment.inputNode.value, "this",
    "The fifth textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(4).attachment.currentExpression, "this",
    "The fifth textbox input value is not the correct one");
}

function test3(scope) {
  is(gWatch.widget._parent.querySelectorAll(".dbg-expression[hidden=true]").length, 4,
    "There should be 4 hidden nodes in the watch expressions container");
  is(gWatch.widget._parent.querySelectorAll(".dbg-expression:not([hidden=true])").length, 0,
    "There should be 0 visible nodes in the watch expressions container");

  ok(scope, "There should be a wach expressions scope in the variables view");
  is(scope._store.size, 4, "There should be 4 evaluations availalble");

  is(gWatch.getItemAtIndex(0).attachment.inputNode.value, "document.title = 43",
    "The first textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(0).attachment.currentExpression, "document.title = 43",
    "The first textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(1).attachment.inputNode.value, "document.title",
    "The second textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(1).attachment.currentExpression, "document.title",
    "The second textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(2).attachment.inputNode.value, "ermahgerd",
    "The third textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(2).attachment.currentExpression, "ermahgerd",
    "The third textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(3).attachment.inputNode.value, "this",
    "The fourth textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(3).attachment.currentExpression, "this",
    "The fourth textbox input value is not the correct one");
}

function test4(scope) {
  is(gWatch.widget._parent.querySelectorAll(".dbg-expression[hidden=true]").length, 3,
    "There should be 3 hidden nodes in the watch expressions container");
  is(gWatch.widget._parent.querySelectorAll(".dbg-expression:not([hidden=true])").length, 0,
    "There should be 0 visible nodes in the watch expressions container");

  ok(scope, "There should be a wach expressions scope in the variables view");
  is(scope._store.size, 3, "There should be 3 evaluations availalble");

  is(gWatch.getItemAtIndex(0).attachment.inputNode.value, "document.title",
    "The first textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(0).attachment.currentExpression, "document.title",
    "The first textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(1).attachment.inputNode.value, "ermahgerd",
    "The second textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(1).attachment.currentExpression, "ermahgerd",
    "The second textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(2).attachment.inputNode.value, "this",
    "The third textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(2).attachment.currentExpression, "this",
    "The third textbox input value is not the correct one");
}

function test5(scope) {
  is(gWatch.widget._parent.querySelectorAll(".dbg-expression[hidden=true]").length, 2,
    "There should be 2 hidden nodes in the watch expressions container");
  is(gWatch.widget._parent.querySelectorAll(".dbg-expression:not([hidden=true])").length, 0,
    "There should be 0 visible nodes in the watch expressions container");

  ok(scope, "There should be a wach expressions scope in the variables view");
  is(scope._store.size, 2, "There should be 2 evaluations availalble");

  is(gWatch.getItemAtIndex(0).attachment.inputNode.value, "ermahgerd",
    "The second textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(0).attachment.currentExpression, "ermahgerd",
    "The second textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(1).attachment.inputNode.value, "this",
    "The third textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(1).attachment.currentExpression, "this",
    "The third textbox input value is not the correct one");
}

function test6(scope) {
  is(gWatch.widget._parent.querySelectorAll(".dbg-expression[hidden=true]").length, 1,
    "There should be 1 hidden nodes in the watch expressions container");
  is(gWatch.widget._parent.querySelectorAll(".dbg-expression:not([hidden=true])").length, 0,
    "There should be 0 visible nodes in the watch expressions container");

  ok(scope, "There should be a wach expressions scope in the variables view");
  is(scope._store.size, 1, "There should be 1 evaluation availalble");

  is(gWatch.getItemAtIndex(0).attachment.inputNode.value, "ermahgerd",
    "The third textbox input value is not the correct one");
  is(gWatch.getItemAtIndex(0).attachment.currentExpression, "ermahgerd",
    "The third textbox input value is not the correct one");
}

function test7(scope) {
  is(gWatch.widget._parent.querySelectorAll(".dbg-expression[hidden=true]").length, 0,
    "There should be 0 hidden nodes in the watch expressions container");
  is(gWatch.widget._parent.querySelectorAll(".dbg-expression:not([hidden=true])").length, 0,
    "There should be 0 visible nodes in the watch expressions container");

  is(scope, undefined, "There should be no watch expressions scope available.");
  is(gWatch.itemCount, 0, "The watch expressions container should be empty.");
}

function addWatchExpression(string) {
  gWatch.addExpression(string);
  gDebugger.editor.focus();
}

function addCmdWatchExpression(string) {
  gWatch._onCmdAddExpression(string);
  gDebugger.editor.focus();
}

function resumeAndFinish() {
  gDebugger.DebuggerController.activeThread.resume(function() {
    closeDebuggerAndFinish();
  });
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebuggee = null;
  gDebugger = null;
  gWatch = null;
  gVars = null;
});
