/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the editing or removing watch expressions works properly.
 */

const TAB_URL = EXAMPLE_URL + "doc_watch-expressions.html";

let gTab, gDebuggee, gPanel, gDebugger;
let gL10N, gEditor, gVars, gWatch;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gL10N = gDebugger.L10N;
    gEditor = gDebugger.DebuggerView.editor;
    gVars = gDebugger.DebuggerView.Variables;
    gWatch = gDebugger.DebuggerView.WatchExpressions;

    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_WATCH_EXPRESSIONS)
      .then(() => testInitialVariablesInScope())
      .then(() => testInitialExpressionsInScope())
      .then(() => testModification("document.title = 42", "document.title = 43", "43", "undefined"))
      .then(() => testIntegrity1())
      .then(() => testModification("aArg", "aArg = 44", "44", "44"))
      .then(() => testIntegrity2())
      .then(() => testModification("aArg = 44", "\ \t\r\ndocument.title\ \t\r\n", "\"43\"", "44"))
      .then(() => testIntegrity3())
      .then(() => testModification("document.title = 43", "\ \t\r\ndocument.title\ \t\r\n", "\"43\"", "44"))
      .then(() => testIntegrity4())
      .then(() => testModification("document.title", "\ \t\r\n", "\"43\"", "44"))
      .then(() => testIntegrity5())
      .then(() => testExprDeletion("this", "44"))
      .then(() => testIntegrity6())
      .then(() => testExprFinalDeletion("ermahgerd", "44"))
      .then(() => testIntegrity7())
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    addExpressions();
    gDebuggee.ermahgerd();
  });
}

function addExpressions() {
  addExpression("this");
  addExpression("ermahgerd");
  addExpression("aArg");
  addExpression("document.title");
  addCmdExpression("document.title = 42");

  is(gWatch.itemCount, 5,
    "There should be 5 items availalble in the watch expressions view.");

  is(gWatch.getItemAtIndex(4).attachment.initialExpression, "this",
    "The first expression's initial value should be correct.");
  is(gWatch.getItemAtIndex(3).attachment.initialExpression, "ermahgerd",
    "The second expression's initial value should be correct.");
  is(gWatch.getItemAtIndex(2).attachment.initialExpression, "aArg",
    "The third expression's initial value should be correct.");
  is(gWatch.getItemAtIndex(1).attachment.initialExpression, "document.title",
    "The fourth expression's initial value should be correct.");
  is(gWatch.getItemAtIndex(0).attachment.initialExpression, "document.title = 42",
    "The fifth expression's initial value should be correct.");

  is(gWatch.getItemAtIndex(4).attachment.currentExpression, "this",
    "The first expression's current value should be correct.");
  is(gWatch.getItemAtIndex(3).attachment.currentExpression, "ermahgerd",
    "The second expression's current value should be correct.");
  is(gWatch.getItemAtIndex(2).attachment.currentExpression, "aArg",
    "The third expression's current value should be correct.");
  is(gWatch.getItemAtIndex(1).attachment.currentExpression, "document.title",
    "The fourth expression's current value should be correct.");
  is(gWatch.getItemAtIndex(0).attachment.currentExpression, "document.title = 42",
    "The fifth expression's current value should be correct.");
}

function testInitialVariablesInScope() {
  let localScope = gVars.getScopeAtIndex(1);
  let argVar = localScope.get("aArg");

  is(argVar.visible, true,
    "Should have the right visibility state for 'aArg'.");
  is(argVar.name, "aArg",
    "Should have the right name for 'aArg'.");
  is(argVar.value.type, "undefined",
    "Should have the right initial value for 'aArg'.");
}

function testInitialExpressionsInScope() {
  let exprScope = gVars.getScopeAtIndex(0);
  let thisExpr = exprScope.get("this");
  let ermExpr = exprScope.get("ermahgerd");
  let argExpr = exprScope.get("aArg");
  let docExpr = exprScope.get("document.title");
  let docExpr2 = exprScope.get("document.title = 42");

  ok(exprScope,
    "There should be a wach expressions scope in the variables view.");
  is(exprScope.name, gL10N.getStr("watchExpressionsScopeLabel"),
    "The scope's name should be marked as 'Watch Expressions'.");
  is(exprScope._store.size, 5,
    "There should be 5 evaluations available.");

  is(thisExpr.visible, true,
    "Should have the right visibility state for 'this'.");
  is(thisExpr.target.querySelectorAll(".variables-view-delete").length, 1,
    "Should have the one close button visible for 'this'.");
  is(thisExpr.name, "this",
    "Should have the right name for 'this'.");
  is(thisExpr.value.type, "object",
    "Should have the right value type for 'this'.");
  is(thisExpr.value.class, "Window",
    "Should have the right value type for 'this'.");

  is(ermExpr.visible, true,
    "Should have the right visibility state for 'ermahgerd'.");
  is(ermExpr.target.querySelectorAll(".variables-view-delete").length, 1,
    "Should have the one close button visible for 'ermahgerd'.");
  is(ermExpr.name, "ermahgerd",
    "Should have the right name for 'ermahgerd'.");
  is(ermExpr.value.type, "object",
    "Should have the right value type for 'ermahgerd'.");
  is(ermExpr.value.class, "Function",
    "Should have the right value type for 'ermahgerd'.");

  is(argExpr.visible, true,
    "Should have the right visibility state for 'aArg'.");
  is(argExpr.target.querySelectorAll(".variables-view-delete").length, 1,
    "Should have the one close button visible for 'aArg'.");
  is(argExpr.name, "aArg",
    "Should have the right name for 'aArg'.");
  is(argExpr.value.type, "undefined",
    "Should have the right value for 'aArg'.");

  is(docExpr.visible, true,
    "Should have the right visibility state for 'document.title'.");
  is(docExpr.target.querySelectorAll(".variables-view-delete").length, 1,
    "Should have the one close button visible for 'document.title'.");
  is(docExpr.name, "document.title",
    "Should have the right name for 'document.title'.");
  is(docExpr.value, "42",
    "Should have the right value for 'document.title'.");

  is(docExpr2.visible, true,
    "Should have the right visibility state for 'document.title = 42'.");
  is(docExpr2.target.querySelectorAll(".variables-view-delete").length, 1,
    "Should have the one close button visible for 'document.title = 42'.");
  is(docExpr2.name, "document.title = 42",
    "Should have the right name for 'document.title = 42'.");
  is(docExpr2.value, 42,
    "Should have the right value for 'document.title = 42'.");

  is(gDebugger.document.querySelectorAll(".dbg-expression[hidden=true]").length, 5,
    "There should be 5 hidden nodes in the watch expressions container.");
  is(gDebugger.document.querySelectorAll(".dbg-expression:not([hidden=true])").length, 0,
    "There should be 0 visible nodes in the watch expressions container.");
}

function testModification(aName, aNewValue, aNewResult, aArgResult) {
  let exprScope = gVars.getScopeAtIndex(0);
  let exprVar = exprScope.get(aName);

  let finished = promise.all([
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES),
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_WATCH_EXPRESSIONS)
  ])
  .then(() => {
    let localScope = gVars.getScopeAtIndex(1);
    let argVar = localScope.get("aArg");

    is(argVar.visible, true,
      "Should have the right visibility state for 'aArg'.");
    is(argVar.target.querySelector(".name").getAttribute("value"), "aArg",
      "Should have the right name for 'aArg'.");
    is(argVar.target.querySelector(".value").getAttribute("value"), aArgResult,
      "Should have the right new value for 'aArg'.");

    let exprScope = gVars.getScopeAtIndex(0);
    let exprOldVar = exprScope.get(aName);
    let exprNewVar = exprScope.get(aNewValue.trim());

    if (!aNewValue.trim()) {
      ok(!exprOldVar,
        "The old watch expression should have been removed.");
      ok(!exprNewVar,
        "No new watch expression should have been added.");
    } else {
      ok(!exprOldVar,
        "The old watch expression should have been removed.");
      ok(exprNewVar,
        "The new watch expression should have been added.");

      is(exprNewVar.visible, true,
        "Should have the right visibility state for the watch expression.");
      is(exprNewVar.target.querySelector(".name").getAttribute("value"), aNewValue.trim(),
        "Should have the right name for the watch expression.");
      is(exprNewVar.target.querySelector(".value").getAttribute("value"), aNewResult,
        "Should have the right new value for the watch expression.");
    }
  });

  let varValue = exprVar.target.querySelector(".title > .name");
  EventUtils.sendMouseEvent({ type: "dblclick" }, varValue, gDebugger);

  let varInput = exprVar.target.querySelector(".title > .element-name-input");
  setText(varInput, aNewValue);
  EventUtils.sendKey("RETURN", gDebugger);

  return finished;
}

function testExprDeletion(aName, aArgResult) {
  let exprScope = gVars.getScopeAtIndex(0);
  let exprVar = exprScope.get(aName);

  let finished = promise.all([
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES),
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_WATCH_EXPRESSIONS)
  ])
  .then(() => {
    let localScope = gVars.getScopeAtIndex(1);
    let argVar = localScope.get("aArg");

    is(argVar.visible, true,
      "Should have the right visibility state for 'aArg'.");
    is(argVar.target.querySelector(".name").getAttribute("value"), "aArg",
      "Should have the right name for 'aArg'.");
    is(argVar.target.querySelector(".value").getAttribute("value"), aArgResult,
      "Should have the right new value for 'aArg'.");

    let exprScope = gVars.getScopeAtIndex(0);
    let exprOldVar = exprScope.get(aName);

    ok(!exprOldVar,
      "The watch expression should have been deleted.");
  });

  let varDelete = exprVar.target.querySelector(".variables-view-delete");
  EventUtils.sendMouseEvent({ type: "click" }, varDelete, gDebugger);

  return finished;
}

function testExprFinalDeletion(aName, aArgResult) {
  let exprScope = gVars.getScopeAtIndex(0);
  let exprVar = exprScope.get(aName);

  let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES).then(() => {
    let localScope = gVars.getScopeAtIndex(0);
    let argVar = localScope.get("aArg");

    is(argVar.visible, true,
      "Should have the right visibility state for 'aArg'.");
    is(argVar.target.querySelector(".name").getAttribute("value"), "aArg",
      "Should have the right name for 'aArg'.");
    is(argVar.target.querySelector(".value").getAttribute("value"), aArgResult,
      "Should have the right new value for 'aArg'.");

    let exprScope = gVars.getScopeAtIndex(0);
    let exprOldVar = exprScope.get(aName);

    ok(!exprOldVar,
      "The watch expression should have been deleted.");
  });

  let varDelete = exprVar.target.querySelector(".variables-view-delete");
  EventUtils.sendMouseEvent({ type: "click" }, varDelete, gDebugger);

  return finished;
}

function testIntegrity1() {
  is(gDebugger.document.querySelectorAll(".dbg-expression[hidden=true]").length, 5,
    "There should be 5 hidden nodes in the watch expressions container.");
  is(gDebugger.document.querySelectorAll(".dbg-expression:not([hidden=true])").length, 0,
    "There should be 0 visible nodes in the watch expressions container.");

  let exprScope = gVars.getScopeAtIndex(0);
  ok(exprScope,
    "There should be a wach expressions scope in the variables view.");
  is(exprScope.name, gL10N.getStr("watchExpressionsScopeLabel"),
    "The scope's name should be marked as 'Watch Expressions'.");
  is(exprScope._store.size, 5,
    "There should be 5 visible evaluations available.");

  is(gWatch.itemCount, 5,
    "There should be 5 hidden expression input available.");
  is(gWatch.getItemAtIndex(0).attachment.inputNode.value, "document.title = 43",
    "The first textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(0).attachment.currentExpression, "document.title = 43",
    "The first textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(1).attachment.inputNode.value, "document.title",
    "The second textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(1).attachment.currentExpression, "document.title",
    "The second textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(2).attachment.inputNode.value, "aArg",
    "The third textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(2).attachment.currentExpression, "aArg",
    "The third textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(3).attachment.inputNode.value, "ermahgerd",
    "The fourth textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(3).attachment.currentExpression, "ermahgerd",
    "The fourth textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(4).attachment.inputNode.value, "this",
    "The fifth textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(4).attachment.currentExpression, "this",
    "The fifth textbox input value is not the correct one.");
}

function testIntegrity2() {
  is(gDebugger.document.querySelectorAll(".dbg-expression[hidden=true]").length, 5,
    "There should be 5 hidden nodes in the watch expressions container.");
  is(gDebugger.document.querySelectorAll(".dbg-expression:not([hidden=true])").length, 0,
    "There should be 0 visible nodes in the watch expressions container.");

  let exprScope = gVars.getScopeAtIndex(0);
  ok(exprScope,
    "There should be a wach expressions scope in the variables view.");
  is(exprScope.name, gL10N.getStr("watchExpressionsScopeLabel"),
    "The scope's name should be marked as 'Watch Expressions'.");
  is(exprScope._store.size, 5,
    "There should be 5 visible evaluations available.");

  is(gWatch.itemCount, 5,
    "There should be 5 hidden expression input available.");
  is(gWatch.getItemAtIndex(0).attachment.inputNode.value, "document.title = 43",
    "The first textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(0).attachment.currentExpression, "document.title = 43",
    "The first textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(1).attachment.inputNode.value, "document.title",
    "The second textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(1).attachment.currentExpression, "document.title",
    "The second textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(2).attachment.inputNode.value, "aArg = 44",
    "The third textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(2).attachment.currentExpression, "aArg = 44",
    "The third textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(3).attachment.inputNode.value, "ermahgerd",
    "The fourth textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(3).attachment.currentExpression, "ermahgerd",
    "The fourth textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(4).attachment.inputNode.value, "this",
    "The fifth textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(4).attachment.currentExpression, "this",
    "The fifth textbox input value is not the correct one.");
}

function testIntegrity3() {
  is(gDebugger.document.querySelectorAll(".dbg-expression[hidden=true]").length, 4,
    "There should be 4 hidden nodes in the watch expressions container.");
  is(gDebugger.document.querySelectorAll(".dbg-expression:not([hidden=true])").length, 0,
    "There should be 0 visible nodes in the watch expressions container.");

  let exprScope = gVars.getScopeAtIndex(0);
  ok(exprScope,
    "There should be a wach expressions scope in the variables view.");
  is(exprScope.name, gL10N.getStr("watchExpressionsScopeLabel"),
    "The scope's name should be marked as 'Watch Expressions'.");
  is(exprScope._store.size, 4,
    "There should be 4 visible evaluations available.");

  is(gWatch.itemCount, 4,
    "There should be 4 hidden expression input available.");
  is(gWatch.getItemAtIndex(0).attachment.inputNode.value, "document.title = 43",
    "The first textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(0).attachment.currentExpression, "document.title = 43",
    "The first textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(1).attachment.inputNode.value, "document.title",
    "The second textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(1).attachment.currentExpression, "document.title",
    "The second textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(2).attachment.inputNode.value, "ermahgerd",
    "The third textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(2).attachment.currentExpression, "ermahgerd",
    "The third textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(3).attachment.inputNode.value, "this",
    "The fourth textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(3).attachment.currentExpression, "this",
    "The fourth textbox input value is not the correct one.");
}

function testIntegrity4() {
  is(gDebugger.document.querySelectorAll(".dbg-expression[hidden=true]").length, 3,
    "There should be 3 hidden nodes in the watch expressions container.");
  is(gDebugger.document.querySelectorAll(".dbg-expression:not([hidden=true])").length, 0,
    "There should be 0 visible nodes in the watch expressions container.");

  let exprScope = gVars.getScopeAtIndex(0);
  ok(exprScope,
    "There should be a wach expressions scope in the variables view.");
  is(exprScope.name, gL10N.getStr("watchExpressionsScopeLabel"),
    "The scope's name should be marked as 'Watch Expressions'.");
  is(exprScope._store.size, 3,
    "There should be 3 visible evaluations available.");

  is(gWatch.itemCount, 3,
    "There should be 3 hidden expression input available.");
  is(gWatch.getItemAtIndex(0).attachment.inputNode.value, "document.title",
    "The first textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(0).attachment.currentExpression, "document.title",
    "The first textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(1).attachment.inputNode.value, "ermahgerd",
    "The second textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(1).attachment.currentExpression, "ermahgerd",
    "The second textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(2).attachment.inputNode.value, "this",
    "The third textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(2).attachment.currentExpression, "this",
    "The third textbox input value is not the correct one.");
}

function testIntegrity5() {
  is(gDebugger.document.querySelectorAll(".dbg-expression[hidden=true]").length, 2,
    "There should be 2 hidden nodes in the watch expressions container.");
  is(gDebugger.document.querySelectorAll(".dbg-expression:not([hidden=true])").length, 0,
    "There should be 0 visible nodes in the watch expressions container.");

  let exprScope = gVars.getScopeAtIndex(0);
  ok(exprScope,
    "There should be a wach expressions scope in the variables view.");
  is(exprScope.name, gL10N.getStr("watchExpressionsScopeLabel"),
    "The scope's name should be marked as 'Watch Expressions'.");
  is(exprScope._store.size, 2,
    "There should be 2 visible evaluations available.");

  is(gWatch.itemCount, 2,
    "There should be 2 hidden expression input available.");
  is(gWatch.getItemAtIndex(0).attachment.inputNode.value, "ermahgerd",
    "The first textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(0).attachment.currentExpression, "ermahgerd",
    "The first textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(1).attachment.inputNode.value, "this",
    "The second textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(1).attachment.currentExpression, "this",
    "The second textbox input value is not the correct one.");
}

function testIntegrity6() {
  is(gDebugger.document.querySelectorAll(".dbg-expression[hidden=true]").length, 1,
    "There should be 1 hidden nodes in the watch expressions container.");
  is(gDebugger.document.querySelectorAll(".dbg-expression:not([hidden=true])").length, 0,
    "There should be 0 visible nodes in the watch expressions container.");

  let exprScope = gVars.getScopeAtIndex(0);
  ok(exprScope,
    "There should be a wach expressions scope in the variables view.");
  is(exprScope.name, gL10N.getStr("watchExpressionsScopeLabel"),
    "The scope's name should be marked as 'Watch Expressions'.");
  is(exprScope._store.size, 1,
    "There should be 1 visible evaluation available.");

  is(gWatch.itemCount, 1,
    "There should be 1 hidden expression input available.");
  is(gWatch.getItemAtIndex(0).attachment.inputNode.value, "ermahgerd",
    "The first textbox input value is not the correct one.");
  is(gWatch.getItemAtIndex(0).attachment.currentExpression, "ermahgerd",
    "The first textbox input value is not the correct one.");
}

function testIntegrity7() {
  is(gDebugger.document.querySelectorAll(".dbg-expression[hidden=true]").length, 0,
    "There should be 0 hidden nodes in the watch expressions container.");
  is(gDebugger.document.querySelectorAll(".dbg-expression:not([hidden=true])").length, 0,
    "There should be 0 visible nodes in the watch expressions container.");

  let localScope = gVars.getScopeAtIndex(0);
  ok(localScope,
    "There should be a local scope in the variables view.");
  isnot(localScope.name, gL10N.getStr("watchExpressionsScopeLabel"),
    "The scope's name should not be marked as 'Watch Expressions'.");
  isnot(localScope._store.size, 0,
    "There should be some variables available.");

  is(gWatch.itemCount, 0,
    "The watch expressions container should be empty.");
}

function addExpression(aString) {
  gWatch.addExpression(aString);
  gEditor.focus();
}

function addCmdExpression(aString) {
  gWatch._onCmdAddExpression(aString);
  gEditor.focus();
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gL10N = null;
  gEditor = null;
  gVars = null;
  gWatch = null;
});
