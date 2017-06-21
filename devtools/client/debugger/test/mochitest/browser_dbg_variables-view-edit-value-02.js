/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the editing variables or properties values works properly.
 */

const TAB_URL = EXAMPLE_URL + "doc_whitespace-property-names.html";

var gDebugger, gVars;

function test() {
  let options = {
    source: TAB_URL,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([tab,, panel]) => {
    gDebugger = panel.panelWin;
    gVars = gDebugger.DebuggerView.Variables;

    let scopes = waitForCaretAndScopes(panel, 26);
    callInTab(tab, "doPause");
    scopes.then(() => {
      let obj = getObjectScope();
      ok(obj, "Should have found the 'obj' variable");
      info("Expanding variable 'obj'");
      let expanded = once(gVars, "fetched");
      obj.expand();
      return expanded;
    }).then(() => initialCheck("\u2028", "\\u2028", "8"))
      .then(() => initialCheck("\u2029", "\\u2029", "9"))
      .then(() => testModification("\u2028", "\\u2028", "123", "123"))
      .then(() => testModification("\u2029", "\\u2029", "456", "456"))
      .then(() => resumeDebuggerThenCloseAndFinish(panel))
      .catch(aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function getObjectScope() {
  return gVars.getScopeAtIndex(0).get("obj");
}

function initialCheck(aProp, aPropReadable, aValue) {
  let aVar = getObjectScope().get(aProp);
  is(aVar.target.querySelector(".value").getAttribute("value"), aValue,
    "Should have the right initial value for '" + aPropReadable + "'.");
}

function testModification(aProp, aPropReadable, aNewValue, aNewResult) {
  let aVar = getObjectScope().get(aProp);

  // Allow the target variable to get painted, so that clicking on
  // its value would scroll the new textbox node into view.
  executeSoon(() => {
    let varValue = aVar.target.querySelector(".title > .value");
    EventUtils.sendMouseEvent({ type: "mousedown" }, varValue, gDebugger);

    let varInput = aVar.target.querySelector(".title > .element-value-input");
    setText(varInput, aNewValue);
    EventUtils.sendKey("RETURN", gDebugger);
  });

  return once(gVars, "fetched").then(() => {
    let aVar = getObjectScope().get(aProp);
    is(aVar.target.querySelector(".value").getAttribute("value"), aNewResult,
        "Should have the right new value for '" + aPropReadable + "'.");
  });
}

registerCleanupFunction(function () {
  gDebugger = gVars = null;
});
