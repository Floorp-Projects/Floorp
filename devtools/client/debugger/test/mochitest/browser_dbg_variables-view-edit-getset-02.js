/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the variables view is able to override getter properties
 * to plain value properties.
 */

const TAB_URL = EXAMPLE_URL + "doc_frame-parameters.html";

var gTab, gPanel, gDebugger;
var gL10N, gEditor, gVars, gWatch;

function test() {
  // Debug test slaves are a bit slow at this test.
  requestLongerTimeout(2);

  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gL10N = gDebugger.L10N;
    gEditor = gDebugger.DebuggerView.editor;
    gVars = gDebugger.DebuggerView.Variables;
    gWatch = gDebugger.DebuggerView.WatchExpressions;

    gVars.switch = function () {};
    gVars.delete = function () {};

    waitForSourceAndCaretAndScopes(gPanel, ".html", 24)
      .then(() => addWatchExpression())
      .then(() => testEdit("\"xlerb\"", "xlerb"))
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    generateMouseClickInTab(gTab, "content.document.querySelector('button')");
  });
}

function addWatchExpression() {
  let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_WATCH_EXPRESSIONS);

  gWatch.addExpression("myVar.prop");
  gEditor.focus();

  return finished;
}

function testEdit(aString, aExpected) {
  let localScope = gVars.getScopeAtIndex(1);
  let myVar = localScope.get("myVar");

  let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_PROPERTIES).then(() => {
    let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_WATCH_EXPRESSIONS).then(() => {
      let exprScope = gVars.getScopeAtIndex(0);

      ok(exprScope,
        "There should be a wach expressions scope in the variables view.");
      is(exprScope.name, gL10N.getStr("watchExpressionsScopeLabel"),
        "The scope's name should be marked as 'Watch Expressions'.");
      is(exprScope._store.size, 1,
        "There should be one evaluation available.");

      is(exprScope.get("myVar.prop").value, aExpected,
        "The expression value is correct after the edit.");
    });

    let editTarget = myVar.get("prop").target;

    // Allow the target variable to get painted, so that clicking on
    // its value would scroll the new textbox node into view.
    executeSoon(() => {
      let varEdit = editTarget.querySelector(".title > .variables-view-edit");
      EventUtils.sendMouseEvent({ type: "mousedown" }, varEdit, gDebugger);

      let varInput = editTarget.querySelector(".title > .element-value-input");
      setText(varInput, aString);
      EventUtils.sendKey("RETURN", gDebugger);
    });

    return finished;
  });

  myVar.expand();
  gVars.clearHierarchy();

  return finished;
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gL10N = null;
  gEditor = null;
  gVars = null;
  gWatch = null;
});

