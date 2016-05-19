/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the editing variables or properties values works properly.
 */

const TAB_URL = EXAMPLE_URL + "doc_frame-parameters.html";

var gTab, gPanel, gDebugger;
var gVars;

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gVars = gDebugger.DebuggerView.Variables;

    waitForSourceAndCaretAndScopes(gPanel, ".html", 24)
      .then(() => initialChecks())
      .then(() => testModification("a", "1"))
      .then(() => testModification("{ a: 1 }", "Object"))
      .then(() => testModification("[a]", "Array[1]"))
      .then(() => testModification("b", "Object"))
      .then(() => testModification("b.a", "1"))
      .then(() => testModification("c.a", "1"))
      .then(() => testModification("Infinity", "Infinity"))
      .then(() => testModification("NaN", "NaN"))
      .then(() => testModification("new Function", "anonymous()"))
      .then(() => testModification("+0", "0"))
      .then(() => testModification("-0", "-0"))
      .then(() => testModification("Object.keys({})", "Array[0]"))
      .then(() => testModification("document.title", '"Debugger test page"'))
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    generateMouseClickInTab(gTab, "content.document.querySelector('button')");
  });
}

function initialChecks() {
  let localScope = gVars.getScopeAtIndex(0);
  let aVar = localScope.get("a");

  is(aVar.target.querySelector(".name").getAttribute("value"), "a",
    "Should have the right name for 'a'.");
  is(aVar.target.querySelector(".value").getAttribute("value"), "1",
    "Should have the right initial value for 'a'.");
}

function testModification(aNewValue, aNewResult) {
  let localScope = gVars.getScopeAtIndex(0);
  let aVar = localScope.get("a");

  // Allow the target variable to get painted, so that clicking on
  // its value would scroll the new textbox node into view.
  executeSoon(() => {
    let varValue = aVar.target.querySelector(".title > .value");
    EventUtils.sendMouseEvent({ type: "mousedown" }, varValue, gDebugger);

    let varInput = aVar.target.querySelector(".title > .element-value-input");
    setText(varInput, aNewValue);
    EventUtils.sendKey("RETURN", gDebugger);
  });

  return waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES).then(() => {
    let localScope = gVars.getScopeAtIndex(0);
    let aVar = localScope.get("a");

    is(aVar.target.querySelector(".name").getAttribute("value"), "a",
      "Should have the right name for 'a'.");
    is(aVar.target.querySelector(".value").getAttribute("value"), aNewResult,
      "Should have the right new value for 'a'.");
  });
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gVars = null;
});
